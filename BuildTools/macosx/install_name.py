import subprocess
import os, sys
from shutil import copyfile

framework_path = sys.argv[1] #e.g. '/Users/xun/Github/geoda/BuildTools/macosx/build/GeoDa.app/Contents/Frameworks'
id = sys.argv[2]

print(id)

def ProcessDependency(dir_path, dylib_name):
    dylib_path = dir_path + '/' + dylib_name
    dep_libs = subprocess.check_output(['otool', '-L', dylib_path]).decode('utf-8')
    items = dep_libs.split('\n')[2:-1]
    for item in items:
        item = item.strip().split(" ")[0]
        if item.startswith('/usr/lib') == False and item.startswith('/System') == False and item.startswith('@executable_path/')==False:
            print("Process:", item)
            file_name = os.path.basename(item)
            # Copy the dylib to Frameworks if needed
            dest = dir_path + '/' + file_name
            if os.path.exists(dest) == False:
                copyfile(item, dest)
            # install_name_tool current item
            new_path = "@executable_path/../Frameworks/{}".format(file_name)
            cmd = "install_name_tool -change \"{}\" \"{}\" {}".format(item, new_path, dylib_path)
            os.system(cmd)
            # process item
            ProcessDependency(dir_path, file_name)
    print("codesign {}", dylib_path)
    cmd = 'codesign --force --timestamp -o runtime -s "{}" {}'.format(id, dylib_path)
    os.system(cmd)


ProcessDependency(framework_path, "libwx_osx_cocoau_gl-3.1.dylib")
ProcessDependency(framework_path, "libwx_osx_cocoau-3.1.dylib")
ProcessDependency(framework_path, "libgdal.29.dylib")