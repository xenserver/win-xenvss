To install the XenServer Volume Shadow Copy Service Provider onto a XenServer 
Windows guest VM:

*    Install xenbus.sys on the guest VM
*    Install xenvbd.sys on the guest VM 
*    Install xeniface.sys on the guest VM
*    Install the xenserver windows guest agent on the guest VM 
*    Copy install-XenProvider.vbs, uninstall-XenProvider.vbs, regvss, 
     vssclient.dll and xenvss.dll onto the guest VM in the same directory 
     that the guest agent was installed
*    As administrator run install-XenProvider.vss

