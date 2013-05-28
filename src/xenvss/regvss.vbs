Option Explicit
On Error Resume Next

'******************************************************************************
' Usage:
'
'   wscript [-register|-unregister][provider name][provider full path][provider desc]
'
'******************************************************************************
Dim Args
Wscript.Interactive = false
Set Args = Wscript.Arguments
CheckError 100

Dim ProviderName, ProviderDLL, ProviderDescription
If Args.Item(0) = "-register" Then 
	ProviderName = Args.Item(1)
	ProviderDLL = Args.Item(2)
	ProviderDescription = Args.Item(3)
	UninstallProvider
	InstallProvider
	Wscript.Quit 0
End If 

If Args.Item(0) = "-unregister" Then 
	ProviderName = Args.Item(1)
	UninstallProvider
	Wscript.Quit 0
End If

Wscript.Quit 0


'******************************************************************************
' Installs the Provider
'******************************************************************************
Sub InstallProvider
	On Error Resume Next

    ' Creating a new COM+ vss hardware provider application
    
    ' Creating the catalog object
	Dim cat
	Set cat = CreateObject("COMAdmin.COMAdminCatalog") 	
	CheckError 101

    ' Get the Applications collection and populate
	Dim collApps
	Set collApps = cat.GetCollection("Applications")
	CheckError 102

	collApps.Populate 
	CheckError 103

    ' Add new application object
	Dim app
	Set app = collApps.Add 
	CheckError 104

    ' Set application name
	app.Value("Name") = ProviderName
	CheckError 105

    ' Set application description
	app.Value("Description") = ProviderDescription 
	CheckError 106

	' Only roles added below are allowed to call in.
	app.Value("ApplicationAccessChecksEnabled") = 1   
	CheckError 107

	' Encrypting communication
	app.Value("Authentication") = 6	                  
	CheckError 108

	' Secure references
	app.Value("AuthenticationCapability") = 2         
	CheckError 109

	' Do not allow impersonation
	app.Value("ImpersonationLevel") = 2               
	CheckError 110

	' Save changes
	collApps.SaveChanges
	CheckError 111

	' Create Windows service running as Local System
	cat.CreateServiceForApplication ProviderName, ProviderName , "SERVICE_AUTO_START", "SERVICE_ERROR_NORMAL", "", ".\localsystem", "", 0
	CheckError 112

	' Add the DLL component"
	cat.InstallComponent ProviderName, ProviderDLL , "", ""
    CheckError 113

	'
	' Add the new role for the Local SYSTEM account
	'

	' Secure the COM+ application
	Dim collRoles
	Set collRoles = collApps.GetCollection("Roles", app.Key)
	CheckError 120

    ' Populate
	collRoles.Populate
	CheckError 121

	' Add new role
	Dim role
	Set role = collRoles.Add
	CheckError 122

	'  Set name = Administrators
	role.Value("Name") = "Administrators"
	CheckError 123

	' Set description = Administrators group
	role.Value("Description") = "Administrators group"
	CheckError 124

	' Save changes
	collRoles.SaveChanges
	CheckError 125
	
	'
	' Add users into role
	'

	' Granting user permissions
	Dim collUsersInRole
	Set collUsersInRole = collRoles.GetCollection("UsersInRole", role.Key)
	CheckError 130

	' Populate
	collUsersInRole.Populate
	CheckError 131

	' Add new user
	Dim user
	Set user = collUsersInRole.Add
	CheckError 132

	' Searching for the Administrators account using WMI

	' Get the Administrators account domain and name
	Dim strQuery
	strQuery = "select * from Win32_Account where SID='S-1-5-32-544' and localAccount=TRUE"
	Dim objSet
	set objSet = GetObject("winmgmts:").ExecQuery(strQuery)
	CheckError 133

	Dim obj, Account
	for each obj in objSet
	    set Account = obj
		exit for
	next

	' Set user name = .\" & Account.Name & " 
	user.Value("User") = ".\" & Account.Name
	CheckError 140

	' Add new user
	Set user = collUsersInRole.Add
	CheckError 141

	' Set user name = Local SYSTEM
	user.Value("User") = "SYSTEM"
	CheckError 142

	' Save changes
	collUsersInRole.SaveChanges
	CheckError 143
	
	Set app      = Nothing
	Set cat      = Nothing
	Set role     = Nothing
	Set user     = Nothing

	Set collApps = Nothing
	Set collRoles = Nothing
	Set collUsersInRole	= Nothing

	set objSet   = Nothing
	set obj      = Nothing 

End Sub


'******************************************************************************
' Uninstalls the Provider
'******************************************************************************
Sub UninstallProvider
	On Error Resume Next

    ' Removing the COM+ vss hardware provider application
    
	Dim cat
	Set cat = CreateObject("COMAdmin.COMAdminCatalog")
	CheckError 201
	
	Dim collApps
	Set collApps = cat.GetCollection("Applications")
	CheckError 202

	collApps.Populate
	CheckError 203
	
	Dim numApps
	numApps = collApps.Count
	Dim i
	For i = numApps - 1 To 0 Step -1
	    If collApps.Item(i).Value("Name") = ProviderName Then
	        collApps.Remove(i)
		CheckError 204
	    End If
	Next
	
	collApps.SaveChanges
	CheckError 205

	Set collApps = Nothing
	Set cat      = Nothing

End Sub



'******************************************************************************
' Sub CheckError
'******************************************************************************
Sub CheckError(exitCode)
    If Err = 0 Then Exit Sub
    Wscript.Quit exitCode
End Sub
