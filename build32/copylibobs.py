import os
import shutil
import sys

print (str(sys.argv))

obspath = sys.argv[1]
dstpath = sys.argv[2]

libobsheader = obspath + '/libobs'
libobsrelease = obspath + '/build32/libobs/RelWithDebInfo'
libobsdebug = obspath + '/build32/libobs/Debug'
libobsd3ddebug = obspath + '/build32/libobs-d3d11/Debug'
libobsd3drelease = obspath + '/build32/libobs-d3d11/RelWithDebInfo'
libobsopengldebug = obspath + '/build32/libobs-opengl/Debug'
libobsopenglrelease = obspath + '/build32/libobs-opengl/RelWithDebInfo'
obsdeps = obspath + '/build32/deps'
obsplugin = obspath + '/build32/plugins'


dstlibobsheader = dstpath + '/libobs'
dstlibobsrelease = dstpath + '/libs/Release'
dstlibobsdebug = dstpath + '/libs/Debug'
dstrundirrelease = dstpath + '/Win32/Release'
dstrundirdebug = dstpath + '/Win32/Debug'
dstrundirplugindebug = dstpath + '/Win32/Debug/obs-plugins/32bit'
dstrundirpluginrelease = dstpath + '/Win32/Release/obs-plugins/32bit'

def copyobsdeps(filepath):
  files = os.listdir(filepath)
  for fi in files:
    fi_d = os.path.join(filepath,fi)
    if os.path.isdir(fi_d):
      cp_tree_ext('dll pdb', fi_d +'/Debug', dstrundirdebug)
      cp_tree_ext('dll pdb', fi_d +'/RelWithDebInfo', dstrundirrelease)
      
def copyobsplugin(filepath):
  files = os.listdir(filepath)
  for fi in files:
    fi_d = os.path.join(filepath,fi)
    if os.path.isdir(fi_d):
      cp_tree_ext('dll pdb', fi_d +'/Debug', dstrundirplugindebug)
      cp_tree_ext('dll pdb', fi_d +'/RelWithDebInfo', dstrundirpluginrelease)

def cp_tree_ext(exts,src,dest):
  """
  Rebuild the director tree like src below dest and copy all files like XXX.exts to dest 
  exts:exetens seperate by blank like "jpg png gif"
  """
  fp={}
  extss=exts.lower().split()
  for dn,dns,fns in os.walk(src):
    for fl in fns:
      if os.path.splitext(fl.lower())[1][1:] in extss:
        if dn not in fp.keys():
          fp[dn]=[]
        fp[dn].append(fl)
  for k,v in fp.items():
      relativepath=k[len(src)+1:]
      newpath=os.path.join(dest,relativepath)
      for f in v:
        oldfile=os.path.join(k,f)
        if not os.path.exists(newpath):
          os.makedirs(newpath)
        shutil.copy(oldfile,newpath)
        
copyobsdeps(obsdeps)
copyobsplugin(obsplugin)
        
cp_tree_ext('h',libobsheader,dstlibobsheader)

cp_tree_ext('lib',libobsrelease,dstlibobsrelease)
cp_tree_ext('dll pdb',libobsrelease,dstrundirrelease)

cp_tree_ext('lib',libobsdebug,dstlibobsdebug)
cp_tree_ext('dll pdb',libobsdebug,dstrundirdebug)

cp_tree_ext('dll pdb',libobsd3drelease,dstrundirrelease)
cp_tree_ext('dll pdb',libobsd3ddebug,dstrundirdebug)
cp_tree_ext('dll pdb',libobsopenglrelease,dstrundirrelease)
cp_tree_ext('dll pdb',libobsopengldebug,dstrundirdebug)

