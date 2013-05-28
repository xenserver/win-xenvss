INSTALL

rem Register VSS hardware provider
cscript "%VBS_DIR%\register_app.vbs" -register "XenVss" "%DLL_DIR%\XenVss.dll" "Citrix XEN VSS Provider"

set EVENT_LOG=HKLM\SYSTEM\CurrentControlSet\Services\Eventlog\Application\XenVss
reg.exe add %EVENT_LOG% /f
reg.exe add %EVENT_LOG% /f /v CustomSource /t REG_DWORD /d 1
reg.exe add %EVENT_LOG% /f /v EventMessageFile /t REG_EXPAND_SZ /d "%DLL_DIR%\XenVss.dll"
reg.exe add %EVENT_LOG% /f /v TypesSupported /t REG_DWORD /d 7


or
UNINSTALL

net stop vds
net stop vss
net stop swprv

reg.exe delete HKLM\SYSTEM\CurrentControlSet\Services\Eventlog\Application\XenVss /f

cscript "%VBS_DIR%\register_app.vbs" -unregister "XenVss"
regsvr32 /s /u "%DLL_DIR%\XenVss.dll"



TEST

use DISKSHADOW (included with svr2008 and above)
e.g.
DISKSHADOW> set context persistent nowriters
DISKSHADOW> set metadata example.cab
DISKSHADOW> set verbose on
DISKSHADOW> begin backup
DISKSHADOW> add volume c: alias systemvolumeshadow
DISKSHADOW> create
DISKSHADOW> expose %systemvolumeshadow% p:
DISKSHADOW> exec c:\backupscript.cmd
DISKSHADOW> end backup



CREATE_SEQUENCE
[AreLunsSupported]
Is SRCLUN(s) supported (i.e. can I get the vdi-uuid from Lun.deviceIdDescriptors)
[BeginPrepareSnapshots]
Add SRCLUN(s).VDI to VDI_UUID list (add SRCDEVICE(s) to internal list?)
[EndPrepareSnapshots]
[PreCommitSnapshots]
[CommitSnapshots]
Write "/vss/<VM_UUID>/snapshot/<VDI_UUID>"=""
Write "/vss/<VM_UUID>/status"="create-snapshots"
Read "/vss/<VM_UUID>/snapshot/<VDI_UUID>/id" => SNAP_UUID
VDI_UUID -> SNAP_UUID
[PostCommitSnapshots]
[GetTargetLuns]
Write "/vss/<VM_UUID>/snapshot/<SNAP_UUID>"=""
Write "/vss/<VM_UUID>/status"="create-snapshotinfo"
Clone Info from SrcLun(s) to DstLun(s), replace VDI_UUID with SNAP_UUID, append SNAP_INFO
[LocateLuns]
[FillInLunInfo]
Is LUN supported (i.e. can I get the vdi-uuid from Lun.deviceIdDescriptors)
[PreFinalCommitSnapshots]
[PostFinalCommitSnapshots]


IMPORT_SEQUENCE
[LocateLuns]
Write "/vss/<VM_UUID>/snapshot/<SNAP_UUID>"=""
Write "/vss/<VM_UUID>/status"="import-snapshots"
[FillInLunInfo]
Is LUN supported (i.e. can I get the vdi-uuid from Lun.deviceIdDescriptors)


DEPORT_SEQUENCE
[FillInLunInfo]
Is LUN supported (i.e. can I get the vdi-uuid from Lun.deviceIdDescriptors)
[OnLunEmpty]
Write "/vss/<VM_UUID>/snapshot/<SNAP_UUID>"=""
Write "/vss/<VM_UUID>/status"="deport-snapshots"
Write "/vss/<VM_UUID>/status"="destroy-snapshots"



[AbortSnapshots]
Write "/vss/<VM_UUID>/snapshot/<SNAP_UUID>"=""
Write "/vss/<VM_UUID>/status"="deport-snapshots"
Write "/vss/<VM_UUID>/status"="destroy-snapshots"

