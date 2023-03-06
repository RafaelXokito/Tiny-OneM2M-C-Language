#
#	FCNT_OL.py
#
#	(c) 2020 by Andreas Kraft
#	License: BSD 3-Clause License. See the LICENSE file for further details.
#
#	ResourceType: oldest (virtual resource) for flexContainer
#

"""	This module implements the virtual <oldest> resource type for <flexContainer> resources.
"""

from __future__ import annotations
from typing import Optional

from ..etc.Types import AttributePolicyDict, ResourceTypes, ResponseStatusCode, Result, JSON, CSERequest
from ..services import CSE
from ..services.Logging import Logging as L
from ..resources.VirtualResource import VirtualResource


class FCNT_OL(VirtualResource):
	"""	This class implements the virtual <oldest> resource for <flexContainer> resources.
	"""

	_allowedChildResourceTypes:list[ResourceTypes] = [ ]
	"""	A list of allowed child-resource types for this resource type. """

	_attributes:AttributePolicyDict = {		
		# None for virtual resources
	}
	""" A dictionary of the attributes and attribute policies for this resource type. 
		The attribute policies are assigned during startup by the `Importer`.
	"""


	def __init__(self, dct:Optional[JSON] = None, 
					   pi:Optional[str] = None, 
					   create:Optional[bool] = False) -> None:
		super().__init__(ResourceTypes.FCNT_OL, dct, pi, create = create, inheritACP = True, readOnly = True, rn = 'ol')


	def handleRetrieveRequest(self, request:Optional[CSERequest] = None, 
									id:Optional[str] = None, 
									originator:Optional[str] = None) -> Result:
		""" Handle a RETRIEVE request.

			Args:
				request: The original request.
				id: Resource ID of the original request.
				originator: The request's originator.

			Return:
				The oldest <flexContainerInstance> for the parent <flexContainer>, or an error `Result`.
		"""
		L.isDebug and L.logDebug('Retrieving oldest FCI from FCNT')
		return self.retrieveLatestOldest(request, originator, ResourceTypes.FCI, oldest = True)


	def handleCreateRequest(self, request:CSERequest, id:str, originator:str) -> Result:
		""" Handle a CREATE request. 

			Args:
				request: The request to process.
				id: The structured or unstructured resource ID of the target resource.
				originator: The request's originator.
			
			Return:
				Fails with error code for this resource type. 
		"""
		return Result.errorResult(rsc = ResponseStatusCode.operationNotAllowed, dbg = 'operation not allowed for <oldest> resource type')


	def handleUpdateRequest(self, request:CSERequest, id:str, originator:str) -> Result:
		""" Handle an UPDATE request.			
	
			Args:
				request: The request to process.
				id: The structured or unstructured resource ID of the target resource.
				originator: The request's originator.
			
			Return:
				Fails with error code for this resource type. 
		"""
		return Result.errorResult(rsc = ResponseStatusCode.operationNotAllowed, dbg = 'operation not allowed for <oldest> resource type')


	def handleDeleteRequest(self, request:CSERequest, id:str, originator:str) -> Result:
		""" Handle a DELETE request.

			Delete the oldest resource.

			Args:
				request: The request to process.
				id: The structured or unstructured resource ID of the target resource.
				originator: The request's originator.
			
			Return:
				Result object indicating success or failure.
		"""
		L.isDebug and L.logDebug('Deleting oldest FCI from FCNT')
		if not (r := CSE.dispatcher.retrieveLatestOldestInstance(self.pi, ResourceTypes.FCI, oldest = True)):
			return Result.errorResult(rsc = ResponseStatusCode.notFound, dbg = 'no instance for <oldest>')
		return CSE.dispatcher.deleteLocalResource(r, originator, withDeregistration = True)
