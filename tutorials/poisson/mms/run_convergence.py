# -*- coding: utf-8 -*-
"""
Simple python script to run convergence test on the MMS poisson example for TF
Created on Fri Aug  9 17:17:51 2013

@author: mspieg@ldeo.columbia.edu
"""

# import general python modules
import os
import subprocess
from math import sqrt
import pylab as pl
import numpy as np
# import TerraFERMA specific modules: PYTHONPATH needs to be set correctly
import libspud
from buckettools.statfile import parser

# set root name
name = "poisson"

#create temporary tfmlfile
tfmlfile=name+"_convergence.tfml"

# number of cells for each problem
ncells = [8, 16, 32, 64, 128]
L2err = np.zeros(len(ncells))

# loop over cells in problem
for i in xrange(len(ncells)):
    # create temporary tfml file with proper number of cells
    n = ncells[i]
    libspud.load_options(name+".tfml")
    libspud.set_option("/geometry/mesh::Mesh/source::UnitSquare/number_cells", [n, n])
    libspud.write_options(tfmlfile)
    
    # build and make program first time
    if i == 0:
        build_dir = 'build_convergence'
        print "Building convergence directory"
        subprocess.call(['tfbuild',tfmlfile,'-d',build_dir])
        os.chdir(build_dir)
        print 'Compiling program'
        subprocess.call(['make','-j','2'])
        os.chdir('..')
        
    # run the progam
    os.chdir(build_dir)
    prog = './poisson_convergence'
    subprocess.call([prog,'../'+tfmlfile,'-l','-vINFO'])
    
    #extract the L2error from the statfile    
    ioname = libspud.get_option("/io/output_base_name")
    stats =  parser(ioname+".stat")
    L2errorSquared = stats['Poisson']['u']['L2NormErrorSquared'][-1]
    L2err[i] = sqrt(L2errorSquared)
    print 'N=', n, 'L2err=', L2err[i]
    os.chdir('..')


# now make  a pretty plot
    
h = 1./np.array(ncells)    
pl.figure()
pl.loglog(h,L2err,'bo-')
pl.xlabel('h')
pl.ylabel('||e||_2')
pl.grid()
p=pl.polyfit(np.log(h),np.log(L2err),1)
pl.title('h Convergence, p={0}'.format(p[0]))
pl.savefig('poisson_convergence.pdf')
pl.show(block=False)