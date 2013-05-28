@echo off

set BASEDIR=%~dp0

net stop vds
net stop vss
net stop swprv
net stop "xensvc"

cscript "%BASEDIR%regvss.vbs" -unregister "XenServerVssProvider"
cscript "%BASEDIR%regvss.vbs" -unregister "XenVssProvider"
cscript "%BASEDIR%regvss.vbs" -unregister "XenVss"
regsvr32 /s /u "%BASEDIR%XenVss.dll"

net start vds
net start vss
net start swprv
net start "xensvc"
