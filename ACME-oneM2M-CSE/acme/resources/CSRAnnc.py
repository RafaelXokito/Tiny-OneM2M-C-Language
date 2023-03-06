#
#	CSRAnnc.py
#
#	(c) 2020 by Andreas Kraft
#	License: BSD 3-Clause License. See the LICENSE file for further details.
#
#	CSR : Announceable variant
#

from __future__ import annotations
from typing import Optional

from ..etc.Types import AttributePolicyDict, ResourceTypes, JSON
from ..resources.AnnouncedResource import AnnouncedResource
from ..resources.Resource import Resource


class CSRAnnc(AnnouncedResource):

	# Specify the allowed child-resource types
	_allowedChildResourceTypes = [	ResourceTypes.ACTR, 
									ResourceTypes.ACTRAnnc,  
									ResourceTypes.CNT, 
									ResourceTypes.CNTAnnc, 
									ResourceTypes.CINAnnc, 
									ResourceTypes.FCNT, 
									ResourceTypes.FCNTAnnc, 
									ResourceTypes.GRP, 
									ResourceTypes.GRPAnnc, 
									ResourceTypes.ACP, 
									ResourceTypes.ACPAnnc,
									ResourceTypes.SUB, 
									ResourceTypes.TS, 
									ResourceTypes.TSAnnc, 
									ResourceTypes.CSRAnnc, 
									ResourceTypes.MGMTOBJAnnc, 
									ResourceTypes.NODAnnc, 
									ResourceTypes.AEAnnc, 
									ResourceTypes.TSB, 
									ResourceTypes.TSBAnnc ]


	# Attributes and Attribute policies for this Resource Class
	# Assigned during startup in the Importer
	_attributes:AttributePolicyDict = {		
		# Common and universal attributes for announced resources
		'rn': None,
		'ty': None,
		'ri': None,
		'pi': None,
		'ct': None,
		'lt': None,
		'et': None,
		'lbl': None,
		'acpi':None,
		'daci': None,
		'ast': None,
		'loc': None,
		'lnk': None,
	
		# Resource attributes
		'cst': None,
		'poa': None,
		'cb': None,
		'csi': None,
		'rr': None,
		'nl': None,
		'csz': None,
		'esi': None,
		'dcse': None,
		'egid': None,
		'mtcc': None,
		'tren': None,
		'ape': None,
		'srv': None
	}
		

	def __init__(self, dct:Optional[JSON] = None, 
					   pi:Optional[str] = None, 
					   create:Optional[bool] = False) -> None:
		super().__init__(ResourceTypes.CSRAnnc, dct, pi = pi, create = create)


