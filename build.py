#!python -u

import os, sys
import datetime
import glob
import shutil
import tarfile
import imp

include = 'include\\'
define = '#define '

brandingName = 'branding'
versionName = 'version'

(brandingFile, brandingPath, brandingDesc) = imp.find_module(
    brandingName, ['src\\' + brandingName])

branding = imp.load_module(
    brandingName, brandingFile, brandingPath, brandingDesc)

def next_build_number():
    try:
        file = open('.build_number', 'r')
        build_number = file.read()
        file.close()
    except IOError:
        build_number = '0'

    file = open('.build_number', 'w')
    file.write(str(int(build_number) + 1))
    file.close()

    return build_number

def make_version_header():
    now = datetime.datetime.now()

    file = open(include + versionName + '.h', 'w')

    for ver in ('MAJOR_VERSION', 'MINOR_VERSION',
                'MICRO_VERSION', 'BUILD_NUMBER'):
        file.write(
            define + ver + '\t' + os.environ[ver] + '\n' +
            define + ver + '_STR\t"' + os.environ[ver] + '"\n' +
            define + ver + '_WSTR\tL"' + os.environ[ver] + '"\n' +
            '\n'
        )

    file.write(define + 'YEAR\t' + str(now.year) + '\n')
    file.write(define + 'YEAR_STR\t"' + str(now.year) + '"\n')

    file.write(define + 'MONTH\t' + str(now.month) + '\n')
    file.write(define + 'MONTH_STR\t"' + str(now.month) + '"\n')

    file.write(define + 'DAY\t' + str(now.day) + '\n')
    file.write(define + 'DAY_STR\t"' + str(now.day) + '"\n')

    file.close()

def make_branding_header():
    file = open(include + brandingName + '.h', 'w')

    for key, value in branding.branding.items():
        file.write(define + 'BRANDING_' + key + ' "' + value + '"\n')

    file.close()

def make_headers():
    make_version_header()
    make_branding_header()

def get_configuration(debug):
    configuration = 'Windows Vista'

    if debug:
        configuration += ' Debug'
    else:
        configuration += ' Release'

    return configuration

def shell(command):
    print(command)
    sys.stdout.flush()

    pipe = os.popen(command, 'r', 1)

    for line in pipe:
        print(line.rstrip())

    return pipe.close()


class msbuild_failure(Exception):
    def __init__(self, value):
        self.value = value
    def __str__(self):
        return repr(self.value)

def msbuild(name, arch, debug):
    cwd = os.getcwd()
    configuration = get_configuration(debug)

    os.environ['SOLUTION'] = name

    if arch == 'x86':
        os.environ['PLATFORM'] = 'Win32'
    elif arch == 'x64':
        os.environ['PLATFORM'] = 'x64'

    os.environ['CONFIGURATION'] = configuration
    os.environ['TARGET'] = 'Build'

    os.chdir('proj')
    status = shell('msbuild.bat')
    os.chdir(cwd)

    if (status != None):
        raise msbuild_failure(configuration)

def archive(name):
    tar = tarfile.open(name + '.tar', 'w')
    tar.add(name)
    tar.close()

def copyfiles(name, arch, debug=False):

    configuration=''
    if debug:
        configuration = 'WindowsVistaDebug'
    else:
        configuration = 'WindowsVistaRelease'
    if arch == 'x86':
        src_path = os.sep.join(['proj', configuration])
    elif arch == 'x64':
        src_path = os.sep.join(['proj', 'x64', configuration ])

    if not os.path.lexists(name):
        os.mkdir(name)

    if arch == 'x86':
        dst_path = os.sep.join([name, 'x86'])
    elif arch == 'x64':
        dst_path = os.sep.join([name, 'x64'])

    if not os.path.lexists(dst_path):
        os.mkdir(dst_path)

    for file in glob.glob(os.sep.join([src_path, '*'])):
        print("%s -> %s" % (file, dst_path))
        shutil.copy(file, dst_path)

    sys.stdout.flush()

def copyfile(name, src, dst):
	src_file = os.sep.join([src, name])
	print("%s -> %s" % (src_file, dst))
	shutil.copy(src_file, dst)
	
def copyscriptfiles(src, dst):
	copyfile('install-XenProvider.cmd', src, dst)
	copyfile('uninstall-XenProvider.cmd', src, dst)
	copyfile('regvss.vbs', src, dst)

if __name__ == '__main__':
    debug = { 'checked': True, 'free': False }

    os.environ['MAJOR_VERSION'] = '7'
    os.environ['MINOR_VERSION'] = '0'
    os.environ['MICRO_VERSION'] = '1'
    if 'BUILD_NUMBER' not in os.environ.keys():
        os.environ['BUILD_NUMBER'] = next_build_number()

    print("BUILD_NUMBER=%s" % os.environ['BUILD_NUMBER'])

    make_headers()

    msbuild('xenvss', 'x86', debug[sys.argv[1]])
    msbuild('xenvss', 'x64', debug[sys.argv[1]])
    copyfiles('xenvss', 'x86', debug[sys.argv[1]])
    copyfiles('xenvss', 'x64', debug[sys.argv[1]])
    copyscriptfiles(os.sep.join(['src', 'xenvss']), 'xenvss')
    archive('xenvss')
