#
#	NotificationManager.py
#
#	(c) 2020 by Andreas Kraft
#	License: BSD 3-Clause License. See the LICENSE file for further details.
#
#	This entity handles subscriptions and sending of notifications. 
#

"""	This module implements the notification manager service functionality for the CSE.
"""

from __future__ import annotations
from typing import Callable, Union, Any, cast, Optional

import sys, copy
from threading import Lock, current_thread

import isodate
from ..etc.Types import CSERequest, MissingData, ResourceTypes, Result, NotificationContentType, NotificationEventType, TimeWindowType
from ..etc.Types import ResponseStatusCode, EventCategory, JSON, JSONLIST, ResourceTypes
from ..etc import Utils, DateUtils
from ..services import CSE
from ..services.Configuration import Configuration
from ..resources.Resource import Resource
from ..resources.CRS import CRS
from ..resources.SUB import SUB
from ..helpers.BackgroundWorker import BackgroundWorker, BackgroundWorkerPool
from ..services.Logging import Logging as L

# TODO: removal policy (e.g. unsuccessful tries)

SenderFunction = Callable[[str], bool]	# type:ignore[misc] # bc cyclic definition 
""" Type definition for sender callback function. """


class NotificationManager(object):
	"""	This class defines functionalities to handle subscriptions and notifications.

		Attributes:
			lockBatchNotification: Internal lock instance for locking certain batch notification methods.
	"""


	def __init__(self) -> None:
		"""	Initialization of a *NotificationManager* instance.
		"""

		# Get the configuration settings
		self._assignConfig()

		# Add handler for configuration updates
		CSE.event.addHandler(CSE.event.configUpdate, self.configUpdate)			# type: ignore

		self.lockBatchNotification = Lock()			# Lock for batchNotifications
		self.lockNotificationEventStats = Lock()	# Lock for notificationEventStats

		CSE.event.addHandler(CSE.event.cseReset, self.restart)		# type: ignore
		L.isInfo and L.log('NotificationManager initialized')


	def shutdown(self) -> bool:
		"""	Shutdown the Notification Manager.
		
			Returns:
				Boolean that indicates the success of the operation
		"""
		L.isInfo and L.log('NotificationManager shut down')
		return True


	def restart(self) -> None:
		"""	Restart the NotificationManager service.
		"""
		L.isInfo and L.log('NotificationManager: Stopping all <CRS> window workers')

		# Stop all crossResourceSubscription workers
		periodicWorkers = BackgroundWorkerPool.stopWorkers('crsPeriodic_*')
		BackgroundWorkerPool.stopWorkers('crsSliding_*')

		# Restart the periodic crossResourceSubscription workers with its old arguments
		for worker in periodicWorkers:
			worker.start(**worker.args)

		L.isDebug and L.logDebug('NotificationManager restarted')


	def _assignConfig(self) -> None:
		self.asyncSubscriptionNotifications	= Configuration.get('cse.asyncSubscriptionNotifications')


	def configUpdate(self, key:Optional[str] = None, 
						   value:Any = None) -> None:
		"""	Handle configuration updates.
		"""
		if key not in [ 'cse.asyncSubscriptionNotifications' ]:
			return
		self._assignConfig()


	###########################################################################
	#
	#	Subscriptions
	#

	def addSubscription(self, subscription:SUB, originator:str) -> Result:
		"""	Add a new subscription. 

			Check each receipient with verification requests.
			
			Args:
				subscription: The new <sub> resource.
				originator: The request originator.
			
			Return:
				Result object.
		"""
		L.isDebug and L.logDebug('Adding subscription')
		if not (res := self._verifyNusInSubscription(subscription, originator = originator)).status:	# verification requests happen here
			return res
		return Result.successResult() if CSE.storage.addSubscription(subscription) \
									  else Result.errorResult(rsc = ResponseStatusCode.internalServerError, dbg = 'cannot add subscription to database')


	def removeSubscription(self, subscription:SUB|CRS, originator:str) -> Result:
		""" Remove a subscription. 

			Send the deletion notifications, if possible.

			Args:
				subscription: The <sub> resource to remove.
			Return:
				Result object.
		"""
		L.isDebug and L.logDebug('Removing subscription')

		# Send outstanding batchNotifications for a subscription
		self._flushBatchNotifications(subscription)

		# Send a deletion request to the subscriberURI
		if (su := subscription.su):
			if not self.sendDeletionNotification(su, subscription.ri):
				L.isWarn and L.logWarn(f'Deletion request failed for: {su}') # but ignore the error

		# Send a deletion request to the associatedCrossResourceSub
		if (acrs := subscription.acrs):
			self.sendDeletionNotification([ nu for nu in acrs ], subscription.ri)
		
		# Finally remove subscriptions from storage
		return Result.successResult() if CSE.storage.removeSubscription(subscription) \
									  else Result.errorResult(rsc = ResponseStatusCode.internalServerError, dbg = 'cannot remove subscription from database')


	def updateSubscription(self, subscription:SUB, previousNus:list[str], originator:str) -> Result:
		"""	Update a subscription.

			This method indirectly updates or rebuild the *notificationStatsInfo* attribute. It should be called
			add the end when updating a subscription.
		
			Args:
				subscription: The <sub> resource to update.
				previousNus: List of previous NUs of the same <sub> resoure.
				originator: The request originator.
			
			Return:
				Result object.
			"""
		L.isDebug and L.logDebug('Updating subscription')
		if not (res := self._verifyNusInSubscription(subscription, previousNus, originator = originator)).status:	# verification requests happen here
			return res
		return Result.successResult() if CSE.storage.updateSubscription(subscription) \
									  else Result.errorResult(rsc = ResponseStatusCode.internalServerError, dbg = 'cannot update subscription in database')


	def getSubscriptionsByNetChty(self, ri:str, 
										net:Optional[list[NotificationEventType]] = None, 
										chty:Optional[ResourceTypes] = None) -> JSONLIST:
		"""	Returns a (possible empty) list of subscriptions for a resource. 
		
			An optional filter can be used 	to return only those subscriptions with a specific enc/net.
			
			Args:
				net: optional filter for enc/net
				chty: optional single child resource typ

			Return:
				List of storage subscription documents, NOT Subscription resources.
			"""
		if not (subs := CSE.storage.getSubscriptionsForParent(ri)):
			return []
		result:JSONLIST = []
		for each in subs:
			if net and any(x in net for x in each['net']):
				result.append(each)
		
		# filter by chty if set
		if chty:
			result = [ each for each in result if (_chty := each['chty']) is None or chty in _chty]

		return result


	def checkSubscriptions(	self, 
							resource:Resource, 
							reason:NotificationEventType, 
							childResource:Optional[Resource] = None, 
							modifiedAttributes:Optional[JSON] = None,
							ri:Optional[str] = None,
							missingData:Optional[dict[str, MissingData]] = None) -> None:
		"""	Check and handle resource events.

			This method looks for subscriptions of a resource and tests, whether an event, like *update* etc, 
			will lead to a notification. It then creates and sends the notification to all targets.

			Args:
				resource: The resource that received the event resp. request.
				reason: The `NotificationEventType` to check.
				childResource: An optional child resource of *resource* that might be updated or created etc.
				modifiedAttributes: An optional `JSON` structure that contains updated attributes.
				ri: Optionally provided resource ID of `Resource`. If it is provided, then *resource* might be *None*.
					It will then be used to retrieve the resource.
				missingData: An optional dictionary of missing data structures in case the *TimeSeries* missing data functionality is handled.
		"""
		
		if resource and resource.isVirtual():
			return 
		if childResource and childResource.isVirtual():
			return
			
		ri = resource.ri if not ri else ri
		L.isDebug and L.logDebug(f'Checking subscriptions ({reason.name}) ri: {ri}')

		# ATTN: The "subscription" returned here are NOT the <sub> resources,
		# but an internal representation from the 'subscription' DB !!!
		# Access to attributes is different bc the structure is flattened
		if (subs := CSE.storage.getSubscriptionsForParent(ri)) is None:
			return
		
		# EXPERIMENTAL Add "subi" subscriptions to the list of subscriptions to check
		if resource and (subi := resource.subi) is not None:
			for eachSubi in subi:
				if (sub := CSE.storage.getSubscription(eachSubi)) is None:
					L.logErr(f'Cannot retrieve subscription: {eachSubi}')
					continue
				# TODO ensure uniqueness
				subs.append(sub)



		# TODO: Add access control check here. Perhaps then the special subscription
		#		DB data structure should go away and be replaced by the normal subscriptions


		for sub in subs:
			# Prevent own notifications for subscriptions 
			ri = sub['ri']
			if childResource and \
				ri == childResource.ri and \
				reason in [ NotificationEventType.createDirectChild, NotificationEventType.deleteDirectChild ]:
					continue
			if reason not in sub['net']:	# check whether reason is actually included in the subscription
				continue
			if reason in [ NotificationEventType.createDirectChild, NotificationEventType.deleteDirectChild ]:	# reasons for child resources
				chty = sub['chty']
				if chty and not childResource.ty in chty:	# skip if chty is set and child.type is not in the list
					continue
				self._handleSubscriptionNotification(sub, 
													 reason, 
													 resource = childResource, 
													 modifiedAttributes = modifiedAttributes, 
													 asynchronous = self.asyncSubscriptionNotifications)
				self.countNotificationEvents(ri)
			
			# Check Update and enc/atr vs the modified attributes 
			elif reason == NotificationEventType.resourceUpdate and (atr := sub['atr']) and modifiedAttributes:
				found = False
				for k in atr:
					if k in modifiedAttributes:
						found = True
				if found:
					self._handleSubscriptionNotification(sub, 
														 reason, 
														 resource = resource, 
														 modifiedAttributes = modifiedAttributes,
														 asynchronous = self.asyncSubscriptionNotifications)
					self.countNotificationEvents(ri)
				else:
					L.isDebug and L.logDebug('Skipping notification: No matching attributes found')
			
			# Check for missing data points (only for <TS>)
			elif reason == NotificationEventType.reportOnGeneratedMissingDataPoints and missingData:
				md = missingData[sub['ri']]
				if md.missingDataCurrentNr >= md.missingDataNumber:	# Always send missing data if the count is greater then the minimum number
					self._handleSubscriptionNotification(sub, 
														 NotificationEventType.reportOnGeneratedMissingDataPoints, 
														 missingData = copy.deepcopy(md),
														 asynchronous = self.asyncSubscriptionNotifications)
					self.countNotificationEvents(ri)
					md.clearMissingDataList()

			elif reason in [NotificationEventType.blockingUpdate, NotificationEventType.blockingRetrieve, NotificationEventType.blockingRetrieveDirectChild]:
				self._handleSubscriptionNotification(sub, 
													 reason, 
													 resource, 
													 modifiedAttributes = modifiedAttributes,
													 asynchronous = False)	# blocking NET always synchronous!
				self.countNotificationEvents(ri)

			else: # all other reasons that target the resource
				self._handleSubscriptionNotification(sub, 
													 reason, 
													 resource, 
													 modifiedAttributes = modifiedAttributes,
													 asynchronous = self.asyncSubscriptionNotifications)
				self.countNotificationEvents(ri)


	def checkPerformBlockingUpdate(self, resource:Resource, 
										 originator:str, 
										 updatedAttributes:JSON, 
										 finished:Optional[Callable] = None) -> Result:
		"""	Check for and perform a *blocking update* request for resource updates that have this event type 
			configured.

			Args:
				resource: The updated resource.
				originator: The originator of the original request.
				updatedAttributes: A structure of all the updated attributes.
				finished: Callable that is called when the notifications were successfully sent and a response was received.
			Returns:
				Result instance indicating success or failure.
		"""
		L.isDebug and L.logDebug('Looking for blocking UPDATE')

		# TODO 2) Prevent or block all other UPDATE request primitives to this target resource.

		# Get blockingUpdate <sub> for this resource , if any, and iterate over them.
		# This should only be one!
		for eachSub in self.getSubscriptionsByNetChty(resource.ri, [NotificationEventType.blockingUpdate]):

			# TODO check notification permission!

			notification:JSON = {
				'm2m:sgn' : {
					'nev' : {
						'net' : NotificationEventType.blockingUpdate.value
					},
					'sur' : Utils.toSPRelative(eachSub['ri'])
				}
			}

			# Check attributes in enc
			if atr := eachSub['atr']:
				jsn, _, _ = Utils.pureResource(updatedAttributes)
				if len(set(jsn.keys()).intersection(atr)) == 0:	# if the intersection between updatedAttributes and the enc/atr contains is empty, then continue
					L.isDebug and L.logDebug(f'skipping <SUB>: {eachSub["ri"]} because configured enc/attribute condition doesn\'t match')
					continue

			# Don't include virtual resources
			if not resource.isVirtual():
				# Add representation
				Utils.setXPath(notification, 'm2m:sgn/nev/rep', updatedAttributes)
				

			# Send notification and handle possible negative response status codes
			if not (res := CSE.request.sendNotifyRequest(eachSub['nus'][0], 
														 originator = CSE.cseCsi,
														 content = notification)).status:
				return res	# Something else went wrong
			if res.rsc == ResponseStatusCode.OK:
				if finished:
					finished()
				continue

			# Modify the result status code for some failure response status codes
			if res.rsc == ResponseStatusCode.targetNotReachable:
				res.dbg = L.logDebug(f'remote entity not reachable: {eachSub["nus"][0]}')
				res.rsc = ResponseStatusCode.remoteEntityNotReachable
				res.status = False
				return res
			elif res.rsc == ResponseStatusCode.operationNotAllowed:
				res.dbg = L.logDebug(f'operation denied by remote entity: {eachSub["nus"][0]}')
				res.rsc = ResponseStatusCode.operationDeniedByRemoteEntity
				res.status = False
				return res
			
			# General negative response status code
			res.status = False
			return res

		# TODO 5) Allow all other UPDATE request primitives for this target resource.

		return Result.successResult()


	def checkPerformBlockingRetrieve(self, 
									 resource:Resource, 
									 request:CSERequest, 
									 finished:Optional[Callable] = None) -> Result:
		"""	Perform a blocking RETRIEVE. If this notification event type is configured a RETRIEVE operation to
			a resource causes a notification to a target. It is expected that the target is updating the resource
			**before** responding to the notification.

			A NOTIFY permission check is done against the originator of the \<subscription> resource, not
			the originator of the request.

			Note:
				This functionality is experimental and not part of the oneM2M spec yet.

			Args:
				resource: The resource that is the target of the RETRIEVE request.
				request: The original request.
				finished: Callable that is called when the notifications were successfully sent and received.
			Return:
				Result instance indicating success or failure.
		"""

		# TODO check notify permission for originator
		# TODO prevent second notification to same 
		# EXPERIMENTAL
		
		L.isDebug and L.logDebug('Looking for blocking RETRIEVE')

		# Get blockingRetrieve <sub> for this resource , if any
		subs = self.getSubscriptionsByNetChty(resource.ri, [NotificationEventType.blockingRetrieve])
		# get and add blockingRetrieveDirectChild <sub> for this resource type, if any
		subs.extend(self.getSubscriptionsByNetChty(resource.pi, [NotificationEventType.blockingRetrieveDirectChild], chty = resource.ty))
		# L.logWarn(resource)

		# Do this for all subscriptions
		countNotifications = 0
		for eachSub in subs:	# This should be only one!
			maxAgeRequest:float = None
			maxAgeSubscription:float = None

			# Check for maxAge attribute provided in the request
			if (maxAgeS := request.fc.attributes.get('ma')) is not None:	# TODO attribute name
				try:
					maxAgeRequest = DateUtils.fromDuration(maxAgeS)
				except Exception as e:
					L.logWarn(dbg := f'error when parsing maxAge in request: {str(e)}')
					return Result.errorResult(dbg = dbg)

			# Check for maxAge attribute provided in the subscription
			if (maxAgeS := eachSub['ma']) is not None:	# EXPERIMENTAL blocking retrieve
				try:
					maxAgeSubscription = DateUtils.fromDuration(maxAgeS)
				except Exception as e:
					L.logWarn(dbg := f'error when parsing maxAge in subscription: {str(e)}')
					return Result.errorResult(dbg = dbg)
				
			# Return if neither the request nor the subscription have a maxAge set
			if maxAgeRequest is None and maxAgeSubscription is None:
				L.isDebug and L.logDebug(f'no maxAge configured, blocking RETRIEVE notification not necessary')
				return Result.successResult()


			# Is either "maxAge" of the request or the subscription reached?
			L.isDebug and L.logDebug(f'request.maxAge: {maxAgeRequest} subscription.maxAge: {maxAgeSubscription}')
			maxAgeSubscription = maxAgeSubscription if maxAgeSubscription is not None else sys.float_info.max
			maxAgeRequest = maxAgeRequest if maxAgeRequest is not None else sys.float_info.max

			if resource.lt > DateUtils.getResourceDate(-int(min(maxAgeRequest, maxAgeSubscription))):
				# To early for this subscription
				continue
			L.isDebug and L.logDebug(f'blocking RETRIEVE notification necessary')

			notification = {
				'm2m:sgn' : {
					'nev' : {
						'net' : eachSub['net'][0],	# Add the first and hopefully only NET to the notification
					},
					'sur' : Utils.toSPRelative(eachSub['ri'])
				}
			}
			# Add creator of the subscription!
			(subOriginator := eachSub['originator']) is not None and Utils.setXPath(notification, 'm2m:sgn/cr', subOriginator)	# Set creator in notification if it was present in subscription

			# Add representation, but don't include virtual resources
			if not resource.isVirtual():
				Utils.setXPath(notification, 'm2m:sgn/nev/rep', resource.asDict())

			countNotifications += 1
			if not (res := CSE.request.sendNotifyRequest(eachSub['nus'][0], 
														 originator = subOriginator,
														 content = notification)).status:
				# TODO: correct RSC according to 7.3.2.9 - see above!
				return res
		
		if countNotifications == 0:
			L.isDebug and L.logDebug(f'No blocking <sub> or too early, no blocking RETRIEVE notification necessary')
			return Result.successResult()
		
		# else
		L.isDebug and L.logDebug(f'Sent {countNotifications} notification(s) for blocking RETRIEVE')
		if finished:
			finished()

		return Result.successResult()


	###########################################################################
	#
	#	CrossResourceSubscriptions
	#

	def addCrossResourceSubscription(self, crs:CRS, originator:str) -> Result:
		"""	Add a new crossResourceSubscription. 
		
			Check each receipient in the *nu* attribute with verification requests. 

			Args:
				crs: The new <crs> resource to check.
				originator: The request originator.
			
			Return:
				Result object.
		"""
		L.isDebug and L.logDebug('Adding crossResourceSubscription')
		if not (res := self._verifyNusInSubscription(crs, originator = originator)).status:	# verification requests happen here
			return res
		return Result.successResult()


	def updateCrossResourceSubscription(self, crs:CRS, previousNus:list[str], originator:str) -> Result:
		"""	Update a crossResourcesubscription. 
		
			Check each new receipient in the *nu* attribute with verification requests. 

			This method indirectly updates or rebuild the *notificationStatsInfo* attribute. It should be called
			add the end when updating a subscription.


			Args:
				crs: The new <crs> resource to check.
				previousNus: A list of the resource's previous NUs.
				originator: The request originator.
			
			Return:
				Result object.
		"""
		L.isDebug and L.logDebug('Updating crossResourceSubscription')
		if not (res := self._verifyNusInSubscription(crs, previousNus, originator = originator)).status:	# verification requests happen here
			return res
		return Result.successResult()


	def removeCrossResourceSubscription(self, crs:CRS) -> Result:
		"""	Remove a crossResourcesubscription. 
		
			Send a deletion request to the *subscriberURI* target.

			Args:
				crs: The new <crs> resource to remove.
			
			Return:
				Result object.
		"""
		L.isDebug and L.logDebug('Removing crossResourceSubscription')

		# Send a deletion request to the subscriberURI
		if (su := crs.su):
			if not self.sendDeletionNotification(su, crs.ri):
				L.isWarn and L.logWarn(f'Deletion request failed for: {su}') # but ignore the error
		return Result.successResult()



	def _crsCheckForNotification(self, data:list[str], crsRi:str, subCount:int) -> None:
		"""	Test whether a notification must be sent for a a <crs> window.

			This method also sends the notification(s) if the window requirements are met.
			
			Args:
				data: List of unique resource IDs.
				crsRi: The resource ID of the <crs> resource for the window.
				subCount: Maximum number of expected resource IDs in *data*.
		"""
		L.isDebug and L.logDebug(f'Checking <crs>: {crsRi} window properties: unique notification count: {len(data)}, expected count: {subCount}')
		if len(data) == subCount:
			L.isDebug and L.logDebug(f'Received sufficient notifications - sending notification')
			if not (res := CSE.dispatcher.retrieveResource(crsRi)).status:
				L.logWarn(f'Cannot retrieve <crs> resource: {crsRi}')	# Not much we can do here
				data.clear()
				return
			crs = cast(CRS, res.resource)

			# Send the notification directly. Handle the counting of sent notifications and received responses
			# in pre and post functions for the notifications of each target
			dct:JSON = { 'm2m:sgn' : {
					'sur' : Utils.toSPRelative(crs.ri)
				}
			}
			self.sendNotificationWithDict(dct, 
										  crs.nu, 
										  originator = CSE.cseCsi,
										  background = True,
										  preFunc = lambda target: self.countSentReceivedNotification(crs, target),
										  postFunc = lambda target: self.countSentReceivedNotification(crs, target, isResponse = True)
										 )
			self.countNotificationEvents(crs.ri, sub = crs)	# Count notification events
			
			# Check for <crs> expiration
			if (exc := crs.exc):
				exc -= 1
				crs.setAttribute('exc', exc)
				L.isDebug and L.logDebug(f'Reducing <crs> expiration counter to {exc}')
				crs.dbUpdate()
				if exc <= 0:
					L.isDebug and L.logDebug(f'<crs>: {crs.ri} expiration counter expired. Deleting resources.')
					CSE.dispatcher.deleteLocalResource(crs, originator = crs.getOriginator())

		data.clear()


	# Time Window Monitor : Periodic

	def _getPeriodicWorkerName(self, ri:str) -> str:
		"""	Return the name of a periodic window worker.
		
			Args:
				ri: Resource ID for which the worker is running.
			
			Return:
				String with the worker name.
		"""
		return f'crsPeriodic_{ri}'


	def startCRSPeriodicWindow(self, crsRi:str, tws:str, expectedCount:int) -> None:

		crsTws = DateUtils.fromDuration(tws)
		L.isDebug and L.logDebug(f'Starting PeriodicWindow for crs: {crsRi}. TimeWindowSize: {crsTws}')

		# Start a background worker. "data", which will contain the RI's later is empty
		BackgroundWorkerPool.newWorker(crsTws, 
									   self._crsPeriodicWindowMonitor, 
									   name = self._getPeriodicWorkerName(crsRi), 
									   startWithDelay = True,
									   data = []).start(crsRi = crsRi, expectedCount = expectedCount)


	def stopCRSPeriodicWindow(self, crsRi:str) -> None:
		L.isDebug and L.logDebug(f'Stopping PeriodicWindow for crs: {crsRi}')
		BackgroundWorkerPool.stopWorkers(self._getPeriodicWorkerName(crsRi))


	def _crsPeriodicWindowMonitor(self, _data:list[str], crsRi:str, expectedCount:int) -> bool: 
		L.isDebug and L.logDebug(f'Checking periodic window for <crs>: {crsRi}')
		self._crsCheckForNotification(_data, crsRi, expectedCount)
		return True


	# Time Window Monitor : Sliding

	def _getSlidingWorkerName(self, ri:str) -> str:
		"""	Return the name of a sliding window worker.
		
			Args:
				ri: Resource ID for which the worker is running.
			
			Return:
				String with the worker name.
		"""
		return f'crsSliding_{ri}'


	def startCRSSlidingWindow(self, crsRi:str, tws:str, sur:str, subCount:int) -> BackgroundWorker:
		crsTws = DateUtils.fromDuration(tws)
		L.isDebug and L.logDebug(f'Starting SlidingWindow for crs: {crsRi}. TimeWindowSize: {crsTws}. SubScount: {subCount}')

		# Start an actor for the sliding window. "data" already contains the first notification source in an array
		return BackgroundWorkerPool.newActor(self._crsSlidingWindowMonitor, 
											 crsTws,
											 name = self._getSlidingWorkerName(crsRi), 
											 data = [ sur ]).start(crsRi = crsRi, subCount = subCount)


	def stopCRSSlidingWindow(self, crsRi:str) -> None:
		L.isDebug and L.logDebug(f'Stopping SlidingWindow for crs: {crsRi}')
		BackgroundWorkerPool.stopWorkers(self._getSlidingWorkerName(crsRi))


	def _crsSlidingWindowMonitor(self, _data:Any, crsRi:str, subCount:int) -> bool:
		L.isDebug and L.logDebug(f'Checking sliding window for <crs>: {crsRi}')
		self._crsCheckForNotification(_data, crsRi, subCount)
		return True


	# Received Notification handling

	def receivedCrossResourceSubscriptionNotification(self, sur:str, crs:Resource) -> None:
		crsRi = crs.ri
		crsTwt = crs.twt
		crsTws = crs.tws
		L.isDebug and L.logDebug(f'Received notification for <crs>: {crsRi}, twt: {crsTwt}, tws: {crsTws}')
		if crsTwt == TimeWindowType.SLIDINGWINDOW:
			if (workers := BackgroundWorkerPool.findWorkers(self._getSlidingWorkerName(crsRi))):
				L.isDebug and L.logDebug(f'Adding notification to worker: {workers[0].name}')
				if sur not in workers[0].data:
					workers[0].data.append(sur)
			else:
				workers = [ self.startCRSSlidingWindow(crsRi, crsTws, sur, crs._countSubscriptions()) ]	# sur is added automatically when creating actor
		elif crsTwt == TimeWindowType.PERIODICWINDOW:
			if (workers := BackgroundWorkerPool.findWorkers(self._getPeriodicWorkerName(crsRi))):
				if sur not in workers[0].data:
					workers[0].data.append(sur)

			# No else: Periodic is running or not

		workers and L.isDebug and L.logDebug(f'Worker data: {workers[0].data}')
		


	###########################################################################
	#
	#	Notifications in general
	#

	def sendNotificationWithDict(self, dct:JSON, 
									   nus:list[str]|str, 
									   originator:Optional[str] = None, 
									   background:Optional[bool] = False, 
									   preFunc:Optional[Callable] = None, 
									   postFunc:Optional[Callable] = None) -> None:
		"""	Send a notification to a single URI or a list of URIs. 
		
			A URI may be a resource ID, then the *poa* of that resource is taken. 
			Also, the serialization is determined when each of the notifications is actually sent.

			Pre- and post-functions can be given that are called before and after sending each
			notification.
			
			Args:
				dct: Dictionary to send as the notification. It already contains the full request.
				nus: A single URI or a list of URIs.
				originator: The originator on which behalf to send the notification. 
				background: Send the notifications in a background task.
				preFunc: Function that is called before each notification sending, with the notification target as a single argument.
				postFunc: Function that is called after each notification sending, with the notification target as a single argument.
		"""

		def _sender(nu: str, originator:str, content:JSON) -> bool:
			if preFunc:
				preFunc(nu)
			CSE.request.sendNotifyRequest(nu, originator = originator, content = content)
			if postFunc:
				postFunc(nu)
			return True

		if isinstance(nus, str):
			nus = [ nus ]
		for nu in nus:
			if background:
				BackgroundWorkerPool.newActor(_sender, 
											  name = f'NO_{current_thread().name}').start(nu = nu, 
																						  originator = originator,
																						  content = dct)
			else:
				_sender(nu, originator = originator, content = dct)


	###########################################################################
	#
	#	Notification Statistics
	#

	def validateAndConstructNotificationStatsInfo(self, sub:SUB|CRS) -> None:
		"""	Update and fill the *notificationStatsInfo* attribute of a \<sub> or \<crs> resource.

			This method adds, if necessary, the necessarry stat info structures for each notification
			URI. It also removes structures for notification URIs that are not present anymore.

			Note:
				For this the *notificationURIs* attribute must be fully validated first.
			
			Args:
				sub: The \<sub> or \<crs> resource for whoich to validate the attribute.
		"""

		if (nsi := sub.nsi) is None:	# nsi attribute must be at least an empty list
			return
		nus = sub.nu

		# Remove from nsi when not in nu (anymore)
		for nsiEntry in list(nsi):
			if nsiEntry['tg'] not in nus:
				nsi.remove(nsiEntry)
		
		# Add new nsi structure for new targets in nu 
		for nu in nus:
			for nsiEntry in nsi:
				if nsiEntry['tg'] == nu:
					break

			# target not found in nsi, add it
			else:
				nsi.append({	'tg': nu,
								'rqs': 0,
								'rsr': 0,
								'noec': 0
							})


	def countSentReceivedNotification(self, sub:SUB|CRS, 
											target:str, 
											isResponse:Optional[bool] = False, 
											count:Optional[int] = 1) -> None:
		"""	If Notification Stats are enabled for a <sub> or <crs> resource, then
			increase the count for sent notifications or received responses.

			Args:
				sub: <sub> or <crs> resource.
				target: URI of the notification target.
				isResponse: Indicates whether a sent notification or a received response should be counted for.
				count: Number of notifications to count.
		"""
		if not sub or not sub.nse:	# Don't count if disabled
			return
		
		L.isDebug and L.logDebug(f'Incrementing notification stats for: {sub.ri} ({"response" if isResponse else "request"})')

		activeField  = 'rsr' if isResponse else 'rqs'
		
		# Search and add to existing target
		# We have to lock this to prevent race conditions in some cases with CRS handling
		with self.lockNotificationEventStats:
			sub.dbReloadDict()	# get a fresh copy of the subscription
			for each in sub.nsi:
				if each['tg'] == target:
					each[activeField] += count
					break
			sub.dbUpdate()


	def countNotificationEvents(self, ri:str, 
									  sub:Optional[SUB|CRS] = None) -> None:
		"""	This method count and stores the number of notification events for a subscription.
			It increments the count for each of the notification targets.

			After handling the resource is updated in the database.
			
			Args:
				ri: Resource ID of a \<sub> or \<csr> resource to handle.
		"""
		if sub is None:
			if not (res := CSE.dispatcher.retrieveLocalResource(ri)).status:
				return
			sub = res.resource
		if not sub.nse:	# Don't count if disabled
			return
		
		L.isDebug and L.logDebug(f'Incrementing notification event stat for: {sub.ri}')
		
		# Search and add to existing target
		# We have to lock this to prevent race conditions in some cases with CRS handling
		with self.lockNotificationEventStats:
			sub.dbReloadDict()	# get a fresh copy of the subscription
			for each in sub.nsi:
				each['noec'] += 1
			sub.dbUpdate()


	def updateOfNSEAttribute(self, sub:CRS|SUB, newNse:bool) -> None:
		""" Handle an update of the *notificationStatsEnable* attribute of a <sub> or <crs>
			resource. 

			Note:
				This removes the *notificationStatsEnable* attribute, which must be added and filled later again, 
				e.g. when validating the *notificationURIs* attribute. 
				For this the *notificationURIs* attribute must be fully validated first.

			Args:
				sub: Either a <sub> or <crs> resource.
				newNse: The new value for the *nse* attribute. This may be empty if not present in the update.
			
		"""
		# nse is not deleted, it is a mandatory attribute
		if newNse is not None:	# present in the request
			oldNse = sub.nse
			if oldNse: # self.nse == True
				if newNse == False:
					pass # Stop collecting, but keep notificationStatsInfo
				else: # Both are True
					sub.setAttribute('nsi', [])
					self.validateAndConstructNotificationStatsInfo(sub)	# nsi is filled here again
			else:	# self.nse == False
				if newNse == True:
					sub.setAttribute('nsi', [])
					self.validateAndConstructNotificationStatsInfo(sub)	# nsi is filled here again
		else:
			# nse is removed (present in resource, but None, and neither True or False)
			sub.delAttribute('nsi')


	#########################################################################


	def _verifyNusInSubscription(self, subscription:SUB|CRS, 
									   previousNus:Optional[list[str]] = None, 
									   originator:Optional[str] = None) -> Result:
		"""	Check all the notification URI's in a subscription. 
		
			A verification request is sent to new URI's. 
			Notifications to the originator are not sent.

			If *previousNus* is given then only new nus are notified.

			Args:
				subscription: <sub> or <crs> resource.
				previousNus: The list of previous NUs.
				originator: The originator on which behalf to send the notification. 
			Return:
				Result object with the overall result of the test.
		"""
		if (nus := subscription.nu):
			ri = subscription.ri
			# notify new nus (verification request). New ones are the ones that are not in the previousNU list
			for nu in nus:
				if not previousNus or (nu not in previousNus):	# send only to new entries in nu
					# Skip notifications to originator
					if nu == originator or Utils.compareIDs(nu, originator):
						L.isDebug and L.logDebug(f'Notification URI skipped: uri: {nu} == originator: {originator}')
						continue
					# Send verification notification to target (either direct URL, or an entity)
					if not self.sendVerificationRequest(nu, ri, originator = originator):
						# Return when even a single verification request fails
						return Result.errorResult(rsc = ResponseStatusCode	.subscriptionVerificationInitiationFailed, dbg = f'Verification request failed for: {nu}')

		# Add/Update NotificationStatsInfo structure
		self.validateAndConstructNotificationStatsInfo(subscription)
		return Result.successResult()


	#########################################################################


	def sendVerificationRequest(self, uri:Union[str, list[str]], 
									  ri:str, 
									  originator:Optional[str] = None) -> bool:
		""""	Define the callback function for verification notifications and send
				the notification.
		"""
		# TODO doc

		def sender(uri:str) -> bool:
			# Skip verification requests to acme: receivers
			if Utils.isAcmeUrl(uri):
				L.isDebug and L.logDebug(f'Skip verification request to internal target: {uri}')
				return True

			L.isDebug and L.logDebug(f'Sending verification request to: {uri}')
			verificationRequest:JSON = {
				'm2m:sgn' : {
					'vrq' : True,
					'sur' : Utils.toSPRelative(ri)
				}
			}
			# Set the creator attribute if there is an originator for the subscription
			originator and Utils.setXPath(verificationRequest, 'm2m:sgn/cr', originator)
	
			if not (res := CSE.request.sendNotifyRequest(uri, 
														 originator = CSE.cseCsi,
														 content = verificationRequest, 
														 noAccessIsError = True)).status:
				L.isDebug and L.logDebug(f'Sending verification request failed for: {uri}: {res.dbg}')
				return False
			if res.rsc != ResponseStatusCode.OK:
				L.isDebug and L.logDebug(f'Verification notification response if not OK: {res.rsc} for: {uri}: {res.dbg}')
				return False
			return True


		return self._sendNotification(uri, sender)


	def sendDeletionNotification(self, uri:Union[str, list[str]], ri:str) -> bool:
		"""	Send a Deletion Notification to a single or a list of target.

			Args:
				uri: Single or a list of notification target URIs.
				ri: ResourceID of the subscription.
			Return:
				Boolean indicat
		"""

		def sender(uri:str) -> bool:
			L.isDebug and L.logDebug(f'Sending deletion notification to: {uri}')
			deletionNotification:JSON = {
				'm2m:sgn' : {
					'sud' : True,
					'sur' : Utils.toSPRelative(ri)
				}
			}

			if not (res := CSE.request.sendNotifyRequest(uri, 
														 originator = CSE.cseCsi,
														 content = deletionNotification)).status:
				L.isDebug and L.logDebug(f'Deletion request failed for: {uri}: {res.dbg}')
				return False
			return True


		return self._sendNotification(uri, sender) if uri else True	# Ignore if the uri is None


	def _handleSubscriptionNotification(self, sub:JSON, 
											  notificationEventType:NotificationEventType, 
											  resource:Optional[Resource] = None, 
											  modifiedAttributes:Optional[JSON] = None, 
											  missingData:Optional[MissingData] = None,
											  asynchronous:bool = False) ->  bool:
		"""	Send a subscription notification.
		"""
		# TODO doc
		L.isDebug and L.logDebug(f'Handling notification for notificationEventType: {notificationEventType}')


		def _sendNotification(uri:str, subscription:SUB, notificationRequest:JSON) -> bool:
			if not CSE.request.sendNotifyRequest(uri, 
												originator = CSE.cseCsi,
												content = notificationRequest).status:
				L.isDebug and L.logDebug(f'Notification failed for: {uri}')
				return False
			self.countSentReceivedNotification(subscription, uri, isResponse = True) # count received notification
			return True


		def sender(uri:str) -> bool:
			"""	Sender callback function for a single normal subscription notifications
			"""
			L.isDebug and L.logDebug(f'Sending notification to: {uri}, reason: {notificationEventType}, asynchronous: {asynchronous}')
			notificationRequest:JSON = {
				'm2m:sgn' : {
					'nev' : {
						'rep' : {},
						'net' : NotificationEventType.resourceUpdate
					},
					'sur' : Utils.toSPRelative(sub['ri'])
				}
			}

			# L.logDebug(missingData)

			nct = sub['nct']
			creator = sub.get('cr')	# creator, might be None
			# switch to populate data
			data = None
			nct == NotificationContentType.all						and (data := resource.asDict())
			nct == NotificationContentType.ri 						and (data := { 'm2m:uri' : resource.ri })
			nct == NotificationContentType.modifiedAttributes		and (data := { resource.tpe : modifiedAttributes })
			nct == NotificationContentType.timeSeriesNotification	and (data := { 'm2m:tsn' : missingData.asDict() })
			# TODO nct == NotificationContentType.triggerPayload

			# Add some values to the notification
			notificationEventType is not None and Utils.setXPath(notificationRequest, 'm2m:sgn/nev/net', notificationEventType)
			data is not None and Utils.setXPath(notificationRequest, 'm2m:sgn/nev/rep', data)
			creator is not None and Utils.setXPath(notificationRequest, 'm2m:sgn/cr', creator)	# Set creator in notification if it was present in subscription

			# Check for batch notifications
			if sub['bn']:
				return self._storeBatchNotification(uri, sub, notificationRequest)
			else:
				# If nse is set to True then count this notification request
				subscription = None
				if sub['nse']:
					if not (res := CSE.dispatcher.retrieveResource(sub['ri'])).status:
						L.logErr(f'Cannot retrieve <sub> resource: {sub["ri"]}: {res.dbg}')
						return False
					subscription = res.resource
					self.countSentReceivedNotification(subscription, uri)	# count sent notification
				
				# Send the notification
				if asynchronous:
					BackgroundWorkerPool.runJob(lambda: _sendNotification(uri, subscription, notificationRequest), 
																		  name = f'NOT_{sub["ri"]}')
					return True
				else:
					return _sendNotification(uri, subscription, notificationRequest)

				# if not CSE.request.sendNotifyRequest(uri, 
				# 									 originator = CSE.cseCsi,
				# 									 content = notificationRequest).status:
				# 	L.isDebug and L.logDebug(f'Notification failed for: {uri}')
				# 	return False
				
				# self.countSentReceivedNotification(subscription, uri, isResponse = True) # count received notification

				return True

		result = self._sendNotification(sub['nus'], sender)	# ! This is not a <sub> resource, but the internal data structure, therefore 'nus

		# Handle subscription expiration in case of a successful notification
		if result and (exc := sub['exc']):
			L.isDebug and L.logDebug(f'Decrement expirationCounter: {exc} -> {exc-1}')

			exc -= 1
			subResource = CSE.storage.retrieveResource(ri=sub['ri']).resource
			if exc < 1:
				L.isDebug and L.logDebug(f'expirationCounter expired. Removing subscription: {subResource.ri}')
				CSE.dispatcher.deleteLocalResource(subResource)	# This also deletes the internal sub
			else:
				subResource.setAttribute('exc', exc)		# Update the exc attribute
				subResource.dbUpdate()						# Update the real subscription
				CSE.storage.updateSubscription(subResource)	# Also update the internal sub
		return result								


	def _sendNotification(self, uris:Union[str, list[str]], senderFunction:SenderFunction) -> bool:
		"""	Send a notification to a single or to multiple targets if necessary. 
		
			Call the infividual callback functions to do the resource preparation and the the actual sending.

			Args:
				uris: Either a string or a list of strings of notification receivers.
				senderFunction: A function that is called to perform the actual notification sending.
			
			Return:
				Returns *True*, even when nothing was sent, and *False* when any *senderFunction* returned False. 
		"""
		#	Event when notification is happening, not sent
		CSE.event.notification() # type: ignore

		if isinstance(uris, str):
			return senderFunction(uris)
		else:
			for uri in uris:
				if not senderFunction(uri):
					return False
			return True



	##########################################################################
	#
	#	Batch Notifications
	#

	def _flushBatchNotifications(self, subscription:Resource) -> None:
		"""	Send and remove any outstanding batch notifications for a subscription.
		"""
		# TODO doc
		L.isDebug and L.logDebug(f'Flush batch notification')

		ri = subscription.ri
		# Get the subscription information (not the <sub> resource itself!).
		# Then get all the URIs/notification targets from that subscription. They might already
		# be filtered.
		if sub := CSE.storage.getSubscription(ri):
			ln = sub['ln'] if 'ln' in sub else False
			for nu in sub['nus']:
				self._stopNotificationBatchWorker(ri, nu)						# Stop a potential worker for that particular batch
				self._sendSubscriptionAggregatedBatchNotification(ri, nu, ln, sub)	# Send all remaining notifications


	def _storeBatchNotification(self, nu:str, sub:JSON, notificationRequest:JSON) -> bool:
		"""	Store a subscription's notification for later sending. For a single nu.
		"""
		# TODO doc
		L.isDebug and L.logDebug(f'Store batch notification nu: {nu}')

		# Rename key name
		if 'm2m:sgn' in notificationRequest:
			notificationRequest['sgn'] = notificationRequest.pop('m2m:sgn')

		# Alway add the notification first before doing the other handling
		ri = sub['ri']
		CSE.storage.addBatchNotification(ri, nu, notificationRequest)

		#  Check for actions
		ln = sub['ln'] if 'ln' in sub else False
		if (num := Utils.findXPath(sub, 'bn/num')) and (cnt := CSE.storage.countBatchNotifications(ri, nu)) >= num:
			L.isDebug and L.logDebug(f'Sending batch notification: bn/num: {num}  countBatchNotifications: {cnt}')

			self._stopNotificationBatchWorker(ri, nu)	# Stop the worker, not needed
			self._sendSubscriptionAggregatedBatchNotification(ri, nu, ln, sub)

		# Check / start Timer worker to guard the batch notification duration
		else:
			try:
				dur = isodate.parse_duration(Utils.findXPath(sub, 'bn/dur')).total_seconds()
			except Exception:
				return False
			self._startNewBatchNotificationWorker(ri, nu, ln, sub, dur)
		return True


	def _sendSubscriptionAggregatedBatchNotification(self, ri:str, nu:str, ln:bool, sub:JSON) -> bool:
		"""	Send and remove(!) the available BatchNotifications for an ri & nu.

			While the sent notifications and the respective received responses are counted here, the
			expiration counter is not. It depends on the events, not the notifications.

			Args:
				ri: Resource ID of the <sub> or <crs> resource.
				nu: A single notification URI.
				ln: *latestNotify*, if *True* then only send the latest notification.
				sub: The internal *sub* structure.
			
			Return:
				Indication of the success of the sending.
		"""
		with self.lockBatchNotification:
			L.isDebug and L.logDebug(f'Sending aggregated subscription notifications for ri: {ri}')

			# Collect the stored notifications for the batch and aggregate them
			notifications = []
			for notification in sorted(CSE.storage.getBatchNotifications(ri, nu), key = lambda x: x['tstamp']):	# type: ignore[no-any-return] # sort by timestamp added
				if n := Utils.findXPath(notification['request'], 'sgn'):
					notifications.append(n)
			if (notificationCount := len(notifications)) == 0:	# This can happen when the subscription is deleted and there are no outstanding notifications
				return False

			parameters:CSERequest = None
			if ln:
				notifications = notifications[-1:]
				# Add event category
				parameters = CSERequest()
				parameters.ec = EventCategory.Latest.value

			# Aggregate and send
			notificationRequest:JSON = {
				'm2m:agn' : {
					 'm2m:sgn' : notifications 
				}
			}

			# Delete old notifications
			if not CSE.storage.removeBatchNotifications(ri, nu):
				L.isWarn and L.logWarn('Error removing aggregated batch notifications')
				return False

			# If nse is set to True then count this notification request
			subscription = None
			nse = sub['nse']
			if nse:
				if not (res := CSE.dispatcher.retrieveResource(sub['ri'])).status:
					L.logErr(f'Cannot retrieve <sub> resource: {sub["ri"]}: {res.dbg}')
					return False
				subscription = res.resource
				self.countSentReceivedNotification(subscription, nu, count = notificationCount)	# count sent notification
				
			# Send the request
			if not CSE.request.sendNotifyRequest(nu, 
												 originator = CSE.cseCsi,
												 content = notificationRequest,
												 parameters = parameters).status:
				L.isWarn and L.logWarn('Error sending aggregated batch notifications')
				return False
			if nse:
				self.countSentReceivedNotification(subscription, nu, isResponse = True, count = notificationCount) # count received notification

			return True


	def _startNewBatchNotificationWorker(self, ri:str, nu:str, ln:bool, sub:JSON, dur:float) -> bool:
		# TODO doc
		if dur is None or dur < 1:	
			L.logErr('BatchNotification duration is < 1')
			return False
		# Check and start a notification worker to send notifications after some time
		if len(BackgroundWorkerPool.findWorkers(self._workerID(ri, nu))) > 0:	# worker started, return
			return True
		L.isDebug and L.logDebug(f'Starting new batchNotificationsWorker. Duration : {dur:f} seconds')
		BackgroundWorkerPool.newActor(self._sendSubscriptionAggregatedBatchNotification, 
									  delay = dur,
									  name = self._workerID(ri, nu)).start(ri = ri, nu = nu, ln = ln, sub = sub)
		return True


	def _stopNotificationBatchWorker(self, ri:str, nu:str) -> None:
		# TODO doc
		BackgroundWorkerPool.stopWorkers(self._workerID(ri, nu))


	def _workerID(self, ri:str, nu:str) -> str:
		"""	Return an ID for a batch notification background worker.
		
			Args:
				ri: ResourceID of a subscription.
				nu: Notification URI of a notification target.
			
			Return:
				String with the ID.
		"""
		return f'{ri};{nu}'

