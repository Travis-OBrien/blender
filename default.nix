### compile process
# nix-shell
# runPhase configurePhase
# runPhase buildPhase
# runPhase installPhase
# blender can be found within:
# /tmp/blender-test/mnt/hdd/git/blender/fork/build/bin

# official instructions from blender.org
# mkdir git/blender/build
# cd to git/blender/build
# cmake ../blender
# if error is thrown about cmaking within a src directory, then delete
# CMakeCache.
# cd to build directory
# make

### required libraries
# INFO:     	Would install group of packages Basics Mandatory Build:
# 			Build Essentials,
# 			GCC,
# 			GCC-C++,
# 			make,
# 			glibc,
# 			Git,
# 			Git LFS,
# 			CMake.
# INFO:     	Would install group of packages Basics Optional Build:
# 			Ninja Builder,
# 			CMake commandline GUI,
# 			CMake GUI,
# 			Patch.
# INFO:     	Would install group of packages Basic Critical Deps:
# 			X11 library,
# 			Xxf86vm Library,
# 			XCursor Library,
# 			Xi Library,
# 			XRandr Library,
# 			Xinerama Library,
# 			XKbCommon Library,
# 			Wayland Library,
# 			Decor Library,
# 			Wayland Protocols,
# 			DBus Library,
# 			OpenGL Library,
# 			EGL Library.

{ pkgs ? import <nixpkgs> {} }:
#with (import <nixpkgs> {});
with pkgs;
let
  libdecor' = libdecor.overrideAttrs (old: {
    # Blender uses private APIs, need to patch to expose them
    patches = (old.patches or [ ]) ++ [ ./libdecor.patch ];
  });
  numpy' = python3.withPackages (ps: with ps; [ numpy cython ]);
  pythonVersion = numpy'.pythonVersion;
in  
mkShell rec {
  pname = "my-blender";
  version = "0.0.1";
  
  cmakeFlags = [
    "-DPYTHON_NUMPY_INCLUDE_DIRS=${numpy'}/${numpy'.sitePackages}/numpy/core/include"
    "-DPYTHON_NUMPY_PATH=${numpy'}/${numpy'.sitePackages}"
    "-DPYTHON_VERSION=${pythonVersion}"

    "-DWITH_PYTHON_INSTALL=OFF"
    "-DWITH_PYTHON_INSTALL_NUMPY=OFF"
    "-DWITH_PYTHON_INSTALL_REQUESTS=OFF"

    # wayland
    "-DWITH_GHOST_WAYLAND=ON"
    "-DWITH_GHOST_WAYLAND_DBUS=ON"
    "-DWITH_GHOST_WAYLAND_DYNLOAD=OFF"
    "-DWITH_GHOST_WAYLAND_LIBDECOR=ON"
  ];

  lib-path = with pkgs;
    lib.makeLibraryPath [ libffi openssl numpy' libdecor'];
  shellHook = ''
    #export PATH=${numpy'}/bin:$PATH
    #export PYTHONPATH=${numpy'}/${numpy'.sitePackages}:$PYTHONPATH
    #export PYTHON_LIBRARY=${numpy'}/lib/libpython${pythonVersion}.so
    #export PYTHON_INCLUDE_DIR=${numpy'}/include/python${pythonVersion}
  # #   # GROK
    export PYTHONPATH=${numpy'}/lib/python${pythonVersion}/${numpy'.sitePackages}:$PYTHONPATH
    export PYTHON_LIBRARY=${numpy'}/lib/libpython${pythonVersion}.so
    export PYTHON_INCLUDE_DIR=${numpy'}/include/python${pythonVersion}

    #export out=/tmp/blender-test
    export NIX_ENFORCE_PURITY=0
    # Augment the dynamic linker path
    #export "LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${lib-path}"
    #export "LIBCLANG_PATH=${libclang.lib}/lib"

    echo "Python path: ${numpy'}"
    echo "Numpy' path: ${numpy'}"
    echo "SitePackages path: ${numpy'.sitePackages}"
    echo "Combined path: ${numpy'}/${numpy'.sitePackages}"
    #echo "Custom env path: $out/lib/python${pythonVersion}/site-packages"

    cd ../build
    '';
  configureFlags = [
    #"-DPYTHON_LIBRARY=${pkgs.python3}/lib/libpython${pkgs.python3.pythonVersion}.so"
    #"-DPYTHON_INCLUDE_DIR=${pkgs.python3}/include/python${pkgs.python3.pythonVersion}"
    #"-DPYTHON_NUMPY'_PATH=${python3Packages.numpy'}"
  ];
  configurePhase = ''
    cmake ../blender
  '';
  buildPhase = ''
    make -j24
  '';
  installPhase = ''
    make install DESTDIR=/tmp/blender-test/
  '';

  # is this needed? check later...
  # postInstall = ''
  #     buildPythonPath "$pythonPath"
  #     wrapProgram $blenderExecutable \
  #       --prefix PATH : $program_PATH \
  #       --prefix PYTHONPATH : "$program_PYTHONPATH" \
  #       --add-flags '--python-use-system-env'
  #   '';

  #export PYTHONPATH=${lib.makeSearchPath python.sitePackages [python311Packages.numpy']}:$PYTHONPATH
  postInstall = ''
      mv $out/share/blender/4.4/python{,-ext}
      wrapProgram $out/mnt/hdd/git/blender/fork/build/bin/blender \
        --prefix PATH : $program_PATH \
        --prefix PYTHONPATH : "${lib.makeSearchPath numpy'.sitePackages [numpy']}" \
        --add-flags '--python-use-system-env'
    '';
  
  # postInstall =
  # ''
  #   wrapProgram $blenderExecutable \
  #     --prefix PATH : $program_PATH
  # '';
  
  nativeBuildInputs = [
    pkgconf
    cmake

    gnumake
    wayland-scanner
    
    shaderc
    vulkan-headers
    vulkan-loader

    bison
    flex
    fontforge
    libiconv
    autoconf
    automake
    libtool
    pkg-config
    makeWrapper
    
    llvmPackages.llvm.dev

    # python
    #python3Packages.wrapPython
    
    autoPatchelfHook
    openexr_3
    libdecor'
  ];
  
  buildInputs = [
    llvmPackages.libcxxStdenv
    clang
        
    pkg-config
    alembic
    boost
    embree
    ffmpeg
    fftw
    fftwFloat
    freetype
    gettext
    glew
    gmp
    git
    git-lfs
    imath
    jemalloc
    libepoxy
    libharu
    libjpeg
    libjpeg_turbo
    libpng
    libsamplerate
    libsndfile
    libtiff
    libwebp
    opencolorio
    
    # probly needs to be in natives, however,
    # it's in builds on git/nixpks.
    # maybe it should be in both natives and builds?
    openexr_3

    openimageio
    openjpeg
    openpgl
    openvdb
    potrace
    pugixml
    
    #python3
    # Include Numpy' directly in Python environment
    numpy'
    #cython
    #python3Packages.numpy'
    
    openssh
    tbb
    zlib
    zstd
    
    # linux
    libGL
    libGLU

    xorg.libX11
    xorg.libXxf86vm
    xorg.libXcursor
    xorg.libXi
    xorg.libXrandr
    xorg.libXinerama
    libxkbcommon
    xorg.libXext
    xorg.libXrender
    xorg.encodings
    
    openal
    openxr-loader

    # wayland
    dbus
    egl-wayland
    wayland
    wayland-protocols
    wayland-scanner
    libdecor'
    #libdecor
    # ?
    wayland-utils
    
    libffi
    
    # vulkan
    shaderc
    vulkan-headers
    vulkan-loader

    tbb
    vulkan-tools
    vulkan-utility-libraries
    
  ];
}
