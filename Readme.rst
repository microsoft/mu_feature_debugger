======================================
Project Mu Debugger Feature Repository
======================================

============================= ================= =============== ===================
 Host Type & Toolchain        Build Status      Test Status     Code Coverage
============================= ================= =============== ===================
Windows_VS_                   |WindowsCiBuild|  |WindowsCiTest| |WindowsCiCoverage|
Ubuntu_GCC5_                  |UbuntuCiBuild|   |UbuntuCiTest|  |UbuntuCiCoverage|
============================= ================= =============== ===================


This repository is part of Project Mu. Please see `Project Mu <https://microsoft.github.io/mu>`_ for details.

This Debugger feature repo contains tools and UEFI code to enable debugging UEFI
implementations through various scenarios and configurations of debugger software
and backends. This repo and contributions to it must adhere to the
`Microsoft Open Source Code of Conduct <https://opensource.microsoft.com/codeofconduct/>`_

Detailed Feature Information
============================

Far more details about using this repo can be found in: `Debugger Feature Package Readme <DebuggerFeaturePkg/Readme.md>`_.

Repository Philosophy
=====================

Unlike other Project MU repositories, the Debugger feature repo does not strictly
follow the EDKII releases, but rather has a continuous main branch which will
periodically receive cherry-picks of needed changes from EDKII. For stable
builds, release tags will be used instead to determine commit hashes at stable
points in development. Release branches may be created as needed to facilitate a
specific release with needed features, but this should be avoided.

Releases Versions
=================

Releases of this repository will follow semantic versioning.

More Info
=========

Please see the `Project Mu docs <https://github.com/Microsoft/mu>`_ for more
information.

This project has adopted the `Microsoft Open Source Code of
Conduct <https://opensource.microsoft.com/codeofconduct/>`_.

For more information see the `Code of Conduct
FAQ <https://opensource.microsoft.com/codeofconduct/faq/>`_ or contact
`opencode@microsoft.com <mailto:opencode@microsoft.com>`_ with any additional
questions or comments.

Issues
======

Please open any issues in the Project Mu GitHub tracker. `More
Details <https://microsoft.github.io/mu/How/contributing/>`_ For Security Issues,
refer to `SECURITY.md <SECURITY.md>`_.

Contributing Code or Docs
=========================

Please follow the general Project Mu Pull Request process.  `More
Details <https://microsoft.github.io/mu/How/contributing/>`_

* `Code Requirements <https://microsoft.github.io/mu/CodeDevelopment/requirements/>`_
* `Doc Requirements <https://microsoft.github.io/mu/DeveloperDocs/requirements/>`_

Builds
======

Please follow the steps in the Project Mu docs to build for CI and local
testing. `More Details <https://microsoft.github.io/mu/CodeDevelopment/compile/>`_

Copyright & License
===================

Copyright (C) Microsoft Corporation
SPDX-License-Identifier: BSD-2-Clause-Patent

.. ===================================================================
.. This is a bunch of directives to make the README file more readable
.. ===================================================================

.. CoreCI

.. _Windows_VS: https://dev.azure.com/projectmu/mu/_build/
.. |WindowsCiBuild| image:: https://img.shields.io/badge/coverage-coming_soon-blue
.. |WindowsCiTest| image:: https://img.shields.io/badge/coverage-coming_soon-blue
.. |WindowsCiCoverage| image:: https://img.shields.io/badge/coverage-coming_soon-blue

.. _Ubuntu_GCC5: https://dev.azure.com/projectmu/mu/_build/
.. |UbuntuCiBuild| image:: https://img.shields.io/badge/coverage-coming_soon-blue
.. |UbuntuCiTest| image:: https://img.shields.io/badge/coverage-coming_soon-blue
.. |UbuntuCiCoverage| image:: https://img.shields.io/badge/coverage-coming_soon-blue
