VSSXen - The XenServer Windows Volume Shadow Copy Service Provider
===================================================================

XenVSS provides a Volume Shadow Copy Service (VSS) provider which is used by
the XenServer guest agent when taking quiesced snapshots of a guest VM

Quick Start
===========

Prerequisites to build
----------------------

*   Visual Studio 2012 or later 
*   Python 3 or later 

Environment variables used in building driver
-----------------------------

BUILD\_NUMBER Build number

PROCESSOR\_ARCHITECTURE x86 or x64

VS location of visual studio

Commands to build
-----------------

    git clone http://github.com/xenserver/win-vss
    cd win-xenvss
    .\build.py [checked | free]

