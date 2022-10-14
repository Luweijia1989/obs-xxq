#!/usr/bin/env python
# -*- coding:utf-8 -*-
import os
import sys
import glob
import subprocess as sp
excludeSet = {'test.exe'}

def sign(file):
    child = sp.Popen(['./SignatureChecker.exe', file], stdout=sp.PIPE)
    streamdata = child.communicate()[0]
    rc = child.returncode
    if rc == 0:
        return
    for i in range(0,3):
        sign_proc = sp.Popen("signtool sign /v /fd sha1 /f pcyuer.pfx /p Xxqpc111! /tr http://rfc3161timestamp.globalsign.com/advanced /td sha256 %s" %(file))
        ret = sign_proc.wait()
        if ret ==0:
            return

for file in glob.glob(os.path.join(sys.argv[1]+'/bin', '*.exe')):
    filepath,filename = os.path.split(file)
    if filename not in excludeSet:
        sign(file)
for file in glob.glob(os.path.join(sys.argv[1]+'/bin', '*.dll')):
    filepath,filename = os.path.split(file)
    if filename not in excludeSet:
        sign(file) 
        
for file in glob.glob(os.path.join(sys.argv[1]+'/plugins/obs-plugins/64bit', '*.exe')):
    filepath,filename = os.path.split(file)
    if filename not in excludeSet:
        sign(file)
for file in glob.glob(os.path.join(sys.argv[1]+'/plugins/obs-plugins/64bit', '*.dll')):
    filepath,filename = os.path.split(file)
    if filename not in excludeSet:
        sign(file) 

for file in glob.glob(os.path.join(sys.argv[1]+'/plugins/data/obs-plugins/enc-amf', '*.exe')):
    filepath,filename = os.path.split(file)
    if filename not in excludeSet:
        sign(file)
        
sign(sys.argv[1]+'/plugins/data/obs-plugins/win-capture/graphics-hook64.dll')
sign(sys.argv[1]+'/plugins/data/obs-plugins/win-capture/graphics-hook32.dll')
sign(sys.argv[1]+'/plugins/data/obs-plugins/win-capture/get-graphics-offsets64.exe')
sign(sys.argv[1]+'/plugins/data/obs-plugins/win-capture/get-graphics-offsets32.exe')