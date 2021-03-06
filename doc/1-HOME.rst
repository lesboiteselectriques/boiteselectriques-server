﻿===========================
Boîtes électriques - Server
===========================

Table of content
================

*1 - HOME*
  Presentation of the application.
  
*2 - PI*
  How to correctly set up the Raspberry Pi

*3 - COMPILING*
  How to build the program from the source.

*4 - SETUP*
  How to install and setup the server's program.

*5 - API*
  How client and server interact with each other.
  (Description of the protocol)
  
General informations
====================

Licenses
~~~~~~~~

This documentation is formatted using the `reStructuredText <http://docutils.sourceforge.net/rst.html>`_ syntax, and is distributed under the Creative Commons BY-SA 4.0 license.

The `Open Sound Control (OSC) packet manipulation library <http://www.rossbencina.com/code/oscpack>`_ is written and distributed under the `MIT license <https://opensource.org/licenses/mit-license.phpl>`_, with the following addition : 

	"Any person wishing to distribute modifications to the Software is requested to send the modifications to `the original developer <mailto:rossb@audiomulch.com>`_ so that they can be incorporated into the canonical version. It is also requested that these non-binding requests be included whenever the above license is reproduced."

The `libaudiotool audio framework <https://github.com/jcelerier/libaudiotool>`_ is distributed under the `GNU GPL v3 <https://www.gnu.org/licenses/gpl-3.0.html>`_.

The application itself is distributed under the `CeCILL 2.1 License <https://www.cecill.info/licences/Licence_CeCILL_V2.1-fr.html>`_.

Notation
~~~~~~~~

In that document, you will often see commands that you can enter in an terminal, beginning either with a ``$`` or a ``#``, that you MUST NOT type.

If the command starts with a ``$``, you just have to be a regular user to perform it (by default, ``pi``).

If it's a ``#``, you NEED ``root`` privileges (sometimes, the ``sudo`` command is enough, but not always).
