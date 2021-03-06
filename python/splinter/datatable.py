# This file is part of the SPLINTER library.
# Copyright (C) 2012 Bjarne Grimstad (bjarne.grimstad@gmail.com).
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from . import splinter
from ctypes import *
from .utilities import *


class DataTable:
	def __init__(self, fileName=None):
		self.__handle = None # Handle to instance in the library
		self.__xDim = None
		self.__numSamples = 0 # Number of samples not yet transferred to back end
		self.__samples = []
		
		if fileName is not None:
			self.__handle = splinter._call(splinter._getHandle().datatable_load_init, getCString(fileName))
			self.__xDim = splinter._call(splinter._getHandle().datatable_get_num_variables, self.__handle)
		else:
			self.__handle = splinter._call(splinter._getHandle().datatable_init)
	
	# "Public" methods (for use by end user of the library)
	def addSample(self, x, y):
		if not isinstance(x, list):
			x = [x]
		
		if self.__xDim is None:
			self.__xDim = len(x)
		
		if self.__xDim != len(x):
			raise Exception("Dimension of the new sample disagrees with the dimension of previous samples!\nPrevious: " + str(self.__xDim) + ", new: " + str(len(x)))
		
		self.__samples += list(x)
		self.__samples += [y]
		self.__numSamples += 1
	
	def getNumVariables(self):
		return self.__xDim
	
	def getNumSamples(self):
		self.__transfer()
		return splinter._call(splinter._getHandle().datatable_get_num_samples, self.__handle)
	
	def save(self, fileName):
		self.__transfer()
		splinter._call(splinter._getHandle().datatable_save, self.__handle, getCString(fileName))
			 
	# Methods below are internal use only
			 
	# Transfer samples to the library
	def __transfer(self):
		#print("Transferring " + str(self.__numSamples) + " samples to backend:")
		#for i in range(self.__numSamples):
		#	print(str(self.__samples[i*(self.__xDim+1)]) + "," + str(self.__samples[i*(self.__xDim+1)+1]) + " = " + str(self.__samples[i*(self.__xDim+1)+2]))
		
		if self.__numSamples > 0:
			splinter._call(splinter._getHandle().datatable_add_samples_row_major, self.__handle, (c_double * len(self.__samples))(*self.__samples), self.__numSamples, self.__xDim)
		
		self.__samples = []
		self.__numSamples = 0
	
	# Getter for the datatable for use by Approximants
	# Will make sure all samples are transferred to the back end before returning the handle to an Approximant
	def _getHandle(self):
		self.__transfer()
		return self.__handle
