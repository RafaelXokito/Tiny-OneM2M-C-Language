#
#	testACTR.py
#
#	(c) 2021 by Andreas Kraft
#	License: BSD 3-Clause License. See the LICENSE file for further details.
#
#	Unit tests for Action functionality
#

import unittest, sys
if '..' not in sys.path:
	sys.path.append('..')
from typing import Tuple
from acme.etc.Types import ResourceTypes as T, ResponseStatusCode as RC, Permission
from init import *


# TODO test: create under cnt without orc


class TestACTR(unittest.TestCase):

	ae 			= None
	ae2 		= None
	cnt			= None
	cntRI		= None
	originator 	= None

	@classmethod
	@unittest.skipIf(noCSE, 'No CSEBase')
	def setUpClass(cls) -> None:
		testCaseStart('Setup TestACTR')
		dct = 	{ 'm2m:ae' : {
					'rn'  : aeRN, 
					'api' : APPID,
				 	'rr'  : True,
				 	'srv' : [ RELEASEVERSION ]
				}}
		cls.ae, rsc = CREATE(cseURL, 'C', T.AE, dct)	# AE to work under
		assert rsc == RC.created, 'cannot create parent AE'
		cls.originator = findXPath(cls.ae, 'm2m:ae/aei')
	
		dct = 	{ 'm2m:cnt' : { 
				'rn' : cntRN,
			}}
		cls.cnt, rsc = CREATE(aeURL, cls.originator, T.CNT, dct)
		assert rsc == RC.created
		cls.cntRI = findXPath(cls.cnt, 'm2m:cnt/ri')
		testCaseEnd('Setup TestACTR')


	@classmethod
	@unittest.skipIf(noCSE, 'No CSEBase')
	def tearDownClass(cls) -> None:
		if not isTearDownEnabled():
			return
		testCaseStart('TearDown TestACTR')
		DELETE(aeURL, ORIGINATOR)	# Just delete the AE and everything below it. Ignore whether it exists or not
		testCaseEnd('TearDown TestACTR')


	def setUp(self) -> None:
		testCaseStart(self._testMethodName)
	

	def tearDown(self) -> None:
		testCaseEnd(self._testMethodName)


	#########################################################################



	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createACTR(self) -> None:
		"""	Create valid <ACTR> """
		self.assertIsNotNone(TestACTR.ae)
		dct = 	{ 'm2m:actr' : { 
					'rn' : actrRN,
					'evc' : { 
						'optr': 1,
						'sbjt': 'rn',
						'thld': 'x'
					},
					'evm': 0,
					'orc': TestACTR.cntRI,
					'apv': {
						# TODO
					} 
				}}
		r, rsc = CREATE(aeURL, ORIGINATOR, T.ACTR, dct)	# Admin, should still fail
		self.assertEqual(rsc, RC.created, r)

		# TODO check attributes
		print(r)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createACTRWrongORCFail(self) -> None:
		"""	Create <ACTR> with wrong ORC -> Fail """
		self.assertIsNotNone(TestACTR.ae)
		dct = 	{ 'm2m:actr' : { 
					'rn' : f'{actrRN}wrong',
					'evc' : { 
						'optr': 1,
						'sbjt': 'rn',
						'thld': 'x'
					},
					'evm': 0,
					'orc': 'todo',
					'apv': {
						# TODO
					} 
				}}
		r, rsc = CREATE(aeURL, ORIGINATOR, T.ACTR, dct)	# Admin, should still fail
		self.assertEqual(rsc, RC.badRequest, r)


	@unittest.skipIf(noCSE, 'No CSEBase')
	def test_createACTRWrongEVCAttributeFail(self) -> None:
		"""	Create <ACTR> with wrong EVC attribute -> Fail """
		self.assertIsNotNone(TestACTR.ae)
		dct = 	{ 'm2m:actr' : { 
					'rn' : f'{actrRN}wrong',
					'evc' : { 
						'optr': 1,
						'sbjt': 'rn',
						'thlt': 'x'	# wrong attribute
					},
					'evm': 0,
					'orc': TestACTR.cntRI,
					'apv': { } 
				}}
		r, rsc = CREATE(aeURL, ORIGINATOR, T.ACTR, dct)	# Admin, should still fail
		self.assertEqual(rsc, RC.badRequest, r)


def run(testFailFast:bool) -> Tuple[int, int, int, float]:
	suite = unittest.TestSuite()
		
	# basic tests
	addTest(suite, TestACTR('test_createACTR'))
	addTest(suite, TestACTR('test_createACTRWrongORCFail'))
	addTest(suite, TestACTR('test_createACTRWrongEVCAttributeFail'))

	result = unittest.TextTestRunner(verbosity=testVerbosity, failfast=testFailFast).run(suite)
	printResult(result)
	return result.testsRun, len(result.errors + result.failures), len(result.skipped), getSleepTimeCount()


if __name__ == '__main__':
	r, errors, s, t = run(True)
	sys.exit(errors)