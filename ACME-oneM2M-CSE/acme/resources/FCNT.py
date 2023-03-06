#
#	FCNT.py
#
#	(c) 2020 by Andreas Kraft
#	License: BSD 3-Clause License. See the LICENSE file for further details.
#
#	ResourceType: FlexContainer
#

from __future__ import annotations
from typing import Optional

from ..etc.Types import AttributePolicyDict, ResourceTypes, Result, ResponseStatusCode, JSON
from ..etc import Utils, DateUtils
from ..services import CSE
from ..services.Logging import Logging as L
from ..resources import Factory as Factory
from ..resources.Resource import Resource
from ..resources.AnnounceableResource import AnnounceableResource


class FCNT(AnnounceableResource):

	# Specify the allowed child-resource types
	_allowedChildResourceTypes = [ ResourceTypes.ACTR, 
								   ResourceTypes.CNT, 
								   ResourceTypes.FCNT, 
								   ResourceTypes.SMD, 
								   ResourceTypes.SUB, 
								   ResourceTypes.TS, 
								   ResourceTypes.FCI ]

	# Attributes and Attribute policies for this Resource Class
	# Assigned during startup in the Importer
	_attributes:AttributePolicyDict = {		
		# Common and universal attributes
		'rn': None,
		'ty': None,
		'ri': None,
		'pi': None,
		'ct': None,
		'lt': None,
		'et': None,
		'lbl': None,
		'cstn': None,
		'acpi':None,
		'at': None,
		'aa': None,
		'ast': None,
		'daci': None,
		'st': None,
		'cr': None,
		'loc': None,

		# Resource attributes
		'cnd': None,
		'or': None,
		'cs': None,
		'nl': None,
		'mni': None,
		'mia': None,
		'mbs': None,
		'cbs': None,
		'cni': None
	}


	_hasFCI	= '__hasFCI__'
	"""	Internal attribute to indicate whether this FCNT has la/ol installed. """

	def __init__(self, dct:Optional[JSON] = None, 
					   pi:Optional[str] = None, 
					   fcntType:Optional[str] = None, 
					   create:Optional[bool] = False) -> None:
		super().__init__(ResourceTypes.FCNT, dct, pi, tpe = fcntType, create = create)
		self._addToInternalAttributes(self._hasFCI)	# Add to internal attributes to ignore in validation etc

		self.setAttribute('cs', 0, overwrite = False)
		self.setAttribute('st', 0, overwrite = False)

		# Indicates whether this FC has flexContainerInstances. 
		# Might change during the lifetime of a resource. Used for optimization
		self._hasInstances 	= False		# not stored in DB
		self.setAttribute(self._hasFCI, False, False)	# stored in DB

		self.__validating = False
		self.ignoreAttributes = self.internalAttributes + [ a for a in self._attributes.keys() ]


	def activate(self, parentResource:Resource, originator:str) -> Result:
		if not (res := super().activate(parentResource, originator)).status:
			return res

		# Add <latest>/<oldest> child resources only when necessary
		if self.hasInstances:	# Set in validate() before
			# register latest and oldest virtual resources
			if not (res := self._addLaOl()).status:
				return res

		return Result.successResult()


	def update(self, dct:Optional[JSON] = None, 
					 originator:Optional[str] = None, 
					 doValidateAttributes:Optional[bool] = True) -> Result:
		
		# Increment stateTag before all because it is needed later to name
		# a FCI, but only when any custom attributes is updated
		for attr in dct:
			if attr not in self.ignoreAttributes:
				self.setAttribute('st', self.st + 1)
				break

		if not (res := super().update(dct, originator)).status:
			return res
		
		# Remove <latest>/<oldest> child resources when necessary (mni etc set to null)
		hasFCI = self[self._hasFCI]
		if self._hasInstances and not hasFCI:
			if not (res := self._addLaOl()).status:
				return res
		elif not self._hasInstances and hasFCI:
			if not (res := self._removeLaOl()).status:
				return res
			if not (res := self._removeFCIs()).status:
				return res
			self.setAttribute('cni', None)
			self.setAttribute('cbs', None)
		
		return Result.successResult()



	# This method is NOT called when adding FCIN!!
	# Because FCInn is added by the FCNT itself.

	def childWillBeAdded(self, childResource:Resource, originator:str) -> Result:
		if not (res := super().childWillBeAdded(childResource, originator)).status:
			return res

		# Check whether the child's rn is "ol" or "la".
		if (rn := childResource['rn']) and rn in ['ol', 'la']:
			return Result.errorResult(rsc = ResponseStatusCode.operationNotAllowed, dbg = 'resource types "latest" or "oldest" cannot be added')
		return Result.successResult()


	# Handle the removal of a FCIN. 
	def childRemoved(self, childResource:Resource, originator:str) -> None:
		super().childRemoved(childResource, originator)
		if childResource.ty == ResourceTypes.FCI:	# Validate if child was FCIN
			self._validateChildren(originator, deletingFCI=True)


	# Checking the presence of cnd and calculating the size
	def validate(self, originator:Optional[str] = None, 
					   create:Optional[bool] = False, 
					   dct:Optional[JSON] = None, 
					   parentResource:Optional[Resource] = None) -> Result:
		if not (res := super().validate(originator, create, dct, parentResource)).status:
			return res
		
		# Validate containerDefinition
		if create:
			if (t := CSE.validator.getFlexContainerSpecialization(self.tpe))[0]:
				if t[0] != self.cnd:
					return Result.errorResult(dbg = L.logDebug(f'Wrong cnd: {self.cnd} for specialization: {self.tpe}. Must be: {t[0]}'))

		# Validate the child resources
		self._validateChildren(originator, dct = dct)
		return Result.successResult()


	def _validateChildren(self, originator:str, 
								deletingFCI:Optional[bool] = False, 
								dct:Optional[JSON] = None) -> None:
		""" Internal validation and checks. This called more often then just from
			the validate() method, for example when deleting a FCIN.
		"""
		# Check whether we already are in validation the children (ie prevent unfortunate recursion by the Dispatcher)
		if self.__validating:
			return
		self.__validating = True

		# Calculate contentSize. Only the custom attribute
		self['cs'] = sum([Utils.getAttributeSize(self[attr]) for attr in self.dict if attr not in self.ignoreAttributes])

		#
		#	Handle flexContainerInstances
		#		

		if self.mni is not None or self.mbs is not None or self.mia is not None: # not when this method is called when already deleting a child resource
			self._hasInstances = True	# Change the internal flag whether this FC has flexContainerInstances

			# Add FCI only 
			# - if mni etc is set, and
			# - if this is NOT a deleteFCI validation, and any of the following is true:
			#   - the _hasFCI attriubte is NOT set (which means we are in progress to add FCI), or
			#   - the update dct is empty, or
			#   - there is any of the custom attributes OR lbl attribute present
			_updateCustomAttributes = dct is not None and any([each not in self.ignoreAttributes or each in [ 'lbl' ] for each in dct[self.tpe].keys()])
			if not deletingFCI and (_updateCustomAttributes or dct is None or not self[self._hasFCI]):
				self.addFlexContainerInstance(originator)
			
			fcis = self.flexContainerInstances()
			cni = len(fcis)	# number of instances

			# check mni
			if (mni := self.mni) is not None:	# is an int
				mni = self.mni
				while cni > mni and cni > 0:
					L.isDebug and L.logDebug(f'cni > mni: Removing <fci>: {fcis[0].ri}')
					# remove oldest
					# Deleting a child must not cause a notification for 'deleteDirectChild'.
					# Don't do a delete check means that FCNT.childRemoved() is not called, where subscriptions for 'deleteDirectChild'  is tested.
					CSE.dispatcher.deleteLocalResource(fcis[0], parentResource = self, doDeleteCheck = False)
					del fcis[0]	# Remove from list
					cni -= 1	# decrement cni when deleting a <fci>

			# Calculate cbs
			cbs = sum([ each.cs for each in fcis])

			# check size
			if (mbs := self.mbs) is not None:
				while cbs > mbs and cbs > 0:
					L.isDebug and L.logDebug(f'cbs > mbs: Removing <fci>: {fcis[0].ri}')
					# remove oldest
					cbs -= fcis[0].cs			
					# Deleting a child must not cause a notification for 'deleteDirectChild'.
					# Don't do a delete check means that FCNT.childRemoved() is not called, where subscriptions for 'deleteDirectChild'  is tested.
					CSE.dispatcher.deleteLocalResource(fcis[0], parentResource = self, doDeleteCheck = False)
					cni -= 1	# again, decrement cbi when deleting a cni

				# Add "current" atribute, if it is not there
				self.setAttribute('cbs', 0, overwrite = False)
			
			self['cni'] = cni
			self['cbs'] = cbs
		else:
			self._hasInstances = False	# Indicate that reqs for child resources is not given
		
		# May have been changed, so store the resource
		self.dbUpdate()
	
		# End validating
		self.__validating = False


	def flexContainerInstances(self) -> list[Resource]:
		"""	Get all flexContainerInstances of a resource and return a sorted (by ct) list
		""" 
		return sorted(CSE.dispatcher.directChildResources(self.ri, ResourceTypes.FCI), key = lambda x: x.ct) # type:ignore[no-any-return]


	# Add a new FlexContainerInstance for this flexContainer
	def addFlexContainerInstance(self, originator:str) -> None:

		L.isDebug and L.logDebug('Adding flexContainerInstance')
		dct:JSON = { 'rn'  : f'{self.rn}_{self.st:d}', }

		# Copy the label as well
		if self.lbl:	# TODO: this is currently (2021/04) not standard conform
			dct['lbl'] = self.lbl

		for attr in self.dict:
			if attr not in self.ignoreAttributes:
				dct[attr] = self[attr]
				continue
			# special for at attribute. It might contain additional id's when it
			# is announced. Those we don't want to copy.
			if attr == 'at':
				dct['at'] = [ x for x in self['at'] if x.count('/') == 1 ]	# Only copy single csi in at

		resource = Factory.resourceFromDict(resDict = { self.tpe : dct }, pi = self.ri, ty = ResourceTypes.FCI).resource
		CSE.dispatcher.createLocalResource(resource, originator = originator)
		resource.setAttribute('cs', self.cs)
		resource.setAttribute('org', originator)

		# Check for mia handling
		if self.mia is not None:	# mia is an int
			# Take either mia or the maxExpirationDelta, whatever is smaller
			maxEt = DateUtils.getResourceDate(self.mia 
											  if self.mia <= CSE.request.maxExpirationDelta 
											  else CSE.request.maxExpirationDelta)
			# Only replace the childresource's et if it is greater than the calculated maxEt
			if resource.et > maxEt:
				resource.setAttribute('et', maxEt)

		resource.dbUpdate()	# store


	def _addLaOl(self) -> Result:
		"""	Add <latest> and <oldest> virtual child resources.
		"""
		L.isDebug and L.logDebug(f'Registering latest and oldest virtual resources for: {self.ri}')

		# add latest
		resource = Factory.resourceFromDict({}, pi = self.ri, ty = ResourceTypes.FCNT_LA).resource	# rn is assigned by resource itself
		# if not (res := CSE.dispatcher.createResource(resource)).resource:
		# 	return Result.errorResult(rsc = res.rsc, dbg = res.dbg)
		if not (res := CSE.dispatcher.createLocalResource(resource)).status:
			return res

		# add oldest
		resource = Factory.resourceFromDict({}, pi = self.ri, ty = ResourceTypes.FCNT_OL).resource	# rn is assigned by resource itself
		# if not (res := CSE.dispatcher.createResource(resource)).resource:
		# 	return Result.errorResult(rsc = res.rsc, dbg = res.dbg)
		if not (res := CSE.dispatcher.createLocalResource(resource)).status:
			return res
		
		self.setAttribute(self._hasFCI, True)
		return Result.successResult()


	def _removeLaOl(self) -> Result:
		"""	Remove <latest> and <oldest> virtual child resources.
		"""
		L.isDebug and L.logDebug(f'De-registering latest and oldest virtual resources for: {self.ri}')

		# remove latest
		if len(res := CSE.dispatcher.directChildResources(self.ri, ResourceTypes.FCNT_LA)) == 1: # type:ignore[no-any-return]
			CSE.dispatcher.deleteLocalResource(res[0])	# ignore errors
		# remove oldest
		if len(res := CSE.dispatcher.directChildResources(self.ri, ResourceTypes.FCNT_OL)) == 1: # type:ignore[no-any-return]
			CSE.dispatcher.deleteLocalResource(res[0])	# ignore errors
	
		self.setAttribute(self._hasFCI, False)
		return Result.successResult()


	def _removeFCIs(self) -> Result:
		"""	Remove the FCI childResources.
		"""
		L.isDebug and L.logDebug(f'Removing FCI child resources for: {self.ri}')
		rs = CSE.dispatcher.directChildResources(self.ri, ty = ResourceTypes.FCI)
		for r in rs:
			# self.childRemoved(r, originator) # It should not be necessary to notify self at this point.
			if not (res := CSE.dispatcher.deleteLocalResource(r, parentResource = self)).status:
				return res
		return Result.successResult()
