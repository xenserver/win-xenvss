

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.00.0595 */
/* Compiler settings for xenvss.idl:
    Oicf, W1, Zp8, env=Win64 (32b run), target_arch=AMD64 8.00.0595 
    protocol : all , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */

#pragma warning( disable: 4049 )  /* more than 64k source lines */


/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 500
#endif

/* verify that the <rpcsal.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCSAL_H_VERSION__
#define __REQUIRED_RPCSAL_H_VERSION__ 100
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif // __RPCNDR_H_VERSION__

#ifndef COM_NO_WINDOWS_H
#include "windows.h"
#include "ole2.h"
#endif /*COM_NO_WINDOWS_H*/

#ifndef __xenvss_i_h__
#define __xenvss_i_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __IXenVssProvider_FWD_DEFINED__
#define __IXenVssProvider_FWD_DEFINED__
typedef interface IXenVssProvider IXenVssProvider;

#endif 	/* __IXenVssProvider_FWD_DEFINED__ */


#ifndef __XenVssProvider_FWD_DEFINED__
#define __XenVssProvider_FWD_DEFINED__

#ifdef __cplusplus
typedef class XenVssProvider XenVssProvider;
#else
typedef struct XenVssProvider XenVssProvider;
#endif /* __cplusplus */

#endif 	/* __XenVssProvider_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"

#ifdef __cplusplus
extern "C"{
#endif 


#ifndef __IXenVssProvider_INTERFACE_DEFINED__
#define __IXenVssProvider_INTERFACE_DEFINED__

/* interface IXenVssProvider */
/* [unique][helpstring][uuid][object] */ 


EXTERN_C const IID IID_IXenVssProvider;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("2DEDAA2B-6D42-42bd-AA28-6015DB5BE972")
    IXenVssProvider : public IUnknown
    {
    public:
    };
    
    
#else 	/* C style interface */

    typedef struct IXenVssProviderVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            __RPC__in IXenVssProvider * This,
            /* [in] */ __RPC__in REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            __RPC__in IXenVssProvider * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            __RPC__in IXenVssProvider * This);
        
        END_INTERFACE
    } IXenVssProviderVtbl;

    interface IXenVssProvider
    {
        CONST_VTBL struct IXenVssProviderVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IXenVssProvider_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IXenVssProvider_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IXenVssProvider_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IXenVssProvider_INTERFACE_DEFINED__ */



#ifndef __XenVssLib_LIBRARY_DEFINED__
#define __XenVssLib_LIBRARY_DEFINED__

/* library XenVssLib */
/* [helpstring][version][uuid] */ 


EXTERN_C const IID LIBID_XenVssLib;

EXTERN_C const CLSID CLSID_XenVssProvider;

#ifdef __cplusplus

class DECLSPEC_UUID("B186CF0B-3420-403e-8752-268A74E8295D")
XenVssProvider;
#endif
#endif /* __XenVssLib_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


