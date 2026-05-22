Building the Editor
====================================

| Below are instructions to build the editor on Linux, Windows, or macOS.
| Note that due to a small dependency on libdragon, GCC is required for now.
| On Windows, that means building via MSYS2.

====================
Prerequisites
====================

Before building the project, make sure you have the following tools installed:

* CMake
* Ninja
* GCC with at least C++23 support
* Git LFS

Linux users should follow the conventions of their distribution and package manager for all packages.

| Windows users need to make sure a recent version of MSYS2_ is installed.
| Open an MSYS2 terminal in the ``UCRT64`` environment, and install the UCRT-specific packages for the dependencies:

.. code-block:: sh

  pacman -S mingw-w64-ucrt-x86_64-gcc
  pacman -S mingw-w64-ucrt-x86_64-cmake
  pacman -S mingw-w64-ucrt-x86_64-ninja
  pacman -S mingw-w64-ucrt-x86_64-python


====================
Git LFS
====================

On some Linux distributions, Git LFS may require adding an external repository to your package manager per these 
`instructions <https://github.com/git-lfs/git-lfs/blob/main/INSTALLING.md>`__.

Windows users should already have Git LFS installed as part of Git for Windows. You can verify this by running:

.. code-block:: sh

  git lfs version

If no version is shown, install Git LFS from their website (https://git-lfs.com/).

After installing Git LFS, initialize it by running:

.. code-block:: sh

  git lfs install

If you already cloned the ``pyrite64`` repository before initalizing Git LFS, navigate to the repository root folder and run:

.. code-block:: sh

  git lfs update


====================
Build Instructions
====================

After cloning the ``pyrite64`` repository, make sure to fetch all the submodules:

.. code-block:: sh

  git submodule update --init --recursive


To configure the project, run:

.. code-block:: sh

  cmake --preset <preset>

After that, and for every subsequent build, run:

.. code-block:: sh

  cmake --build --preset <preset>

Where ``<preset>`` is replaced with the CMake preset name corresponding to your system:

* ``linux-release`` for Linux systems, release version
* ``linux-debug`` for Linux systems, debug version
* ``windows-gcc-release`` for Windows systems with MSYS2, release version
* ``windows-gcc-debug`` for Windows systems with MSYS2, debug version
* ``macos-release`` for macOS systems, release version

| Once the build is finished, a program called ``pyrite64`` (or ``pyrite64.exe``) should be placed in the root directory of the repo.
| The program itself can be placed anywhere on the system, however the ``./data`` and ``./n64`` directories must stay next to it.

To open the editor, simply execute ``./pyrite64`` (or ``.\pyrite64.exe``).


====================
macOS
====================

macOS requires a few extra steps compared to Linux and Windows, because the N64 toolchain (mips64-elf compiler, libdragon, Tiny3D) must be built from source.

Prerequisites
-------------

Before building, make sure the following are installed:

* **Xcode Command Line Tools** — provides ``clang``, ``git``, ``make``:

  .. code-block:: sh

    xcode-select --install

* **Homebrew** — required by the in-app toolchain installer (https://brew.sh):

  .. code-block:: sh

    /bin/bash -c "$(curl -fsSL https://brew.sh/install.sh)"

* **CMake and Ninja** via Homebrew:

  .. code-block:: sh

    brew install cmake ninja

* **Git LFS** via Homebrew:

  .. code-block:: sh

    brew install git-lfs

Clone and build
---------------

.. code-block:: sh

  git clone <repo-url> pyrite64
  cd pyrite64
  git submodule update --init --recursive
  git lfs install && git lfs pull
  cmake --preset macos-release
  cmake --build --preset macos-release

.. note::
  ``git lfs pull`` is required. If skipped, binary assets (fonts, textures) will be corrupted stubs and the editor will crash on launch.

Run the editor
--------------

.. code-block:: sh

  ./pyrite64.app/Contents/MacOS/pyrite64

Install the N64 toolchain
--------------------------

On first launch, the **Toolchain Manager** will open automatically. Click **Install** — a Terminal window will open and run the automated installer, which:

1. Installs Homebrew build dependencies (``gmp``, ``mpfr``, ``gcc``, etc.)
2. Builds the MIPS64 cross-compiler from source (~15–30 minutes)
3. Clones and builds libdragon (``preview`` branch)
4. Clones and builds Tiny3D and its GLTF importer tool

Leave the Terminal window open until it finishes. Once complete, add the following to your shell profile (``~/.zshrc`` or ``~/.bash_profile``):

.. code-block:: sh

  export N64_INST="$HOME/.local/n64"
  export PATH="$N64_INST/bin:$PATH"

Then restart the editor. All four steps in the Toolchain Manager should show green.

.. note::
  The toolchain install path defaults to ``$HOME/.local/n64``. If you already have a toolchain elsewhere, set ``N64_INST`` to that path before launching the installer.


Known issues
----------

**AppleClang narrowing error in surface.h**

If you update the ``vendored/libdragon`` submodule to a newer upstream commit,
AppleClang may reject the build with an error like::

  error: narrowing conversion of 'format' from 'tex_format_t' to 'uint16_t'

This is because AppleClang enforces C++11 narrowing rules more strictly than GCC.
The upstream libdragon source may not include the required cast.

To fix it, open ``vendored/libdragon/include/surface.h`` and find the line (~172)
inside the ``surface_make`` function that reads:

.. code-block:: c

  .flags = format,

Change it to:

.. code-block:: c

  .flags = (uint16_t)format,

This fix is already applied in the pinned submodule commit. It only needs to be
re-applied if you bump the submodule to a newer upstream commit that reverts it.


.. _MSYS2: https://www.msys2.org/