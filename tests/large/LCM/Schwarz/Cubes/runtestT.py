
#! /usr/bin/env python

import sys
import os
import re
from subprocess import Popen

result = 0

######################
# Test 1 
######################
print "test 1 - Cubes DBC"
name = "Cubes_DBC"
log_file_name = name + ".log"
if os.path.exists(log_file_name):
    os.remove(log_file_name)
logfile = open(log_file_name, 'w')

#specify tolerance to determine test failure / passing
tolerance = 1.0e-9; 
meanvalue = 0.000809523809524;

# run AlbanyT 
command = ["./AlbanyT", "cubes.yaml"]
p = Popen(command, stdout=logfile, stderr=logfile)
return_code = p.wait()
if return_code != 0:
    result = return_code

for line in open(log_file_name):
  if "Main_Solve: MeanValue of final solution" in line:
    s = line
    s = line[40:]
    d = float(s)
    print d
    if (d > meanvalue + tolerance or d < meanvalue - tolerance):
      result = result+1 

if result != 0:
    print "result is %s" % result
    print "%s test has failed" % name
    sys.exit(result)

with open(log_file_name, 'r') as log_file:
    print log_file.read() 

######################
# Test 2 
######################
print "test 1 - Cubes SDBC"
name = "Cubes_SDBC"
log_file_name = name + ".log"
if os.path.exists(log_file_name):
    os.remove(log_file_name)
logfile = open(log_file_name, 'w')

#specify tolerance to determine test failure / passing
tolerance = 1.0e-9; 
meanvalue = 0.000809523809521;

# run AlbanyT 
command = ["./AlbanyT", "cubes.yaml"]
p = Popen(command, stdout=logfile, stderr=logfile)
return_code = p.wait()
if return_code != 0:
    result = return_code

for line in open(log_file_name):
  if "Main_Solve: MeanValue of final solution" in line:
    s = line
    s = line[40:]
    d = float(s)
    print d
    if (d > meanvalue + tolerance or d < meanvalue - tolerance):
      result = result+1 

if result != 0:
    print "result is %s" % result
    print "%s test has failed" % name
    sys.exit(result)

with open(log_file_name, 'r') as log_file:
    print log_file.read() 


sys.exit(result)

