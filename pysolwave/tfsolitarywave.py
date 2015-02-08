"""python class  for calculating solitary waves given input from TerraFERMA .tfml files
   uses libspud to read the tfml files and then populate
"""

__author__ = "Marc Spiegelman (mspieg@ldeo.columbia.edu)"
__date__ = "8 February 2015"
__copyright__ = "Copyright (C) 2010 Marc Spiegelman"
__license__  = "GNU LGPL Version 2.1"

import numpy as np
from solitarywave import SolitaryWave
import libspud
import sys

class TFSolitaryWave:
    """ class for calculating and evaluating solitary wave profiles with data from TerraFERMA .tfml input files
    """

    def __init__(self, tfml_file,system_name='magma',c_name='c',n_name='n',m_name='m',d_name='d',N_name='N',h_squared_name='h_squared',x0_name='x0'):
        """read the tfml_file and use libspud to populate the internal parameters

        c: wavespeed
        n: permeability exponent
        m: bulk viscosity exponent
        d: wave dimension
        N: number of collocation points
        x0: coordinate wave peak
        h:  the size of the system in compaction lengths
        """
        # initialize libspud and extract parameters
        libspud.load_options(tfml_file)
        # get model dimension
        self.dim = libspud.get_option("/geometry/dimension")
        # get solitary wave parameters
        path="/system::"+system_name+"/coefficient::"
        scalar_value="/type::Constant/rank::Scalar/value::WholeMesh/constant"
        vector_value="/type::Constant/rank::Vector/value::WholeMesh/constant::dim"
        c = libspud.get_option(path+c_name+scalar_value)
        n = int(libspud.get_option(path+n_name+scalar_value))
        m = int(libspud.get_option(path+m_name+scalar_value))
        d = int(libspud.get_option(path+d_name+scalar_value))
        N = int(libspud.get_option(path+N_name+scalar_value))
        self.h = np.sqrt(libspud.get_option(path+h_squared_name+scalar_value))
        self.x0 = np.array(libspud.get_option(path+x0_name+vector_value))
        self.swave = SolitaryWave(c,n,m,d,N)
        self.rmax = self.swave.r[-1]
  
        # check that d <= dim
        assert (d <= self.dim)   
        
        # sort out appropriate index for calculating distance r
        if d == 1:
            self.index = [self.dim - 1]
        else: 
            self.index = range(0,d)  
            
        # check that the origin point is the correct dimension
        assert (len(self.x0) == self.dim)
        
    def getr(self,x):
        """ return radial position with respect to wave origin x0
        for solitarywave of dimension d in overall dimension dim
        assuming x is a numpy array of points of dimension d"""
        
        # check that x is the right shaped numpy array
        if len(x.shape) == 1:             # 1-D array
            if self.dim == 1:
                npnts = x.shape[0]
            else:
                assert(x.shape == self.x0.shape)
                npnts = 1
        else:  #n-D array
            assert(x.shape[-1] == self.x0.shape[0])
            npnts = x.shape[0]
                    
        if npnts == 1:
            dx = x[self.index] - self.x0[self.index]
            r = self.h*np.sqrt(np.sum(dx*dx))
        else:
            dx = x[:,self.index] - self.x0[self.index]
            r = self.h*np.sqrt(np.sum(dx*dx,1))

        return r

    def eval(self,x):
        """ calculate position r given numpy array of x-coordinates
            and return appropriate solitary wave amplitude
        """
        

        # calculate radial coordinate r of x
        r = self.getr(x)
        # check if r is a scalar
        if np.isscalar(r):
            if r > self.rmax:
                f = 1.
            else:
                f = self.swave.interp(r)
        else:
            f = np.ones(r.shape)
            omega = r <= self.rmax # r within collocation domain
            f[omega] = self.swave.finterp(r[omega])
        
        return f