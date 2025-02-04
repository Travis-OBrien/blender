# blender can be found within:
# /mnt/hdd/git/blender-clone/build/mnt/hdd/git/blender-clone/build/bin
# this is hacky and dirty, but it works for now.

# scuffed instructions but produces binary
# make lite
# cd to build dir (ex: git/blender/build)
# cmake .
# make
# at this point, blender should be in build/bin
# if we run 'make install' we get an error

# official instructions
# mkdir git/blender/build
# cd to git/blender/build
# cmake ../blender
# if error is thrown about cmaking within a src directory, then delete
# CMakeCache.
# cd to build directory
# make

# cmake -D WITH_IMAGE_OPENEXR=OFF WITH_PYTHON_INSTALL_NUMPY ../blender


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

with (import <nixpkgs> {});
let
    libdecor' = libdecor.overrideAttrs (old: {
      # Blender uses private APIs, need to patch to expose them
      patches = (old.patches or [ ]) ++ [ ./libdecor.patch ];
    });
    python3 = python311Packages.python;
    #python3Packages = python311Packages;
  in  
#mkShell.override { stdenv = pkgs.llvmPackages_15.stdenv; }
mkShell rec {
  

  # disable all hardening flags
  #NIX_HARDENING_ENABLE = "";
  # enable default hardening flags
  #NIX_HARDENING_ENABLE = "pie pic format stackprotector fortify strictoverflow";
  #NIX_HARDENING_ENABLE = "";
  
  pname = "my-blender";
  version = "4.3.2";

  # postUnpack = ''
  #   chmod -R u+w *
  #   rm -r assets/working
  #   mv assets --target-directory source/release/datafiles/
  # '';

  # sourceRoot = "source";

  # ???
  #preBuild = "set -x";



  #configurePhase
  
  env.NIX_CFLAGS_COMPILE = "-I${python3}/include/${python3.libPrefix}";
  # NIX_CFLAGS_COMPILE = [
  #   # ensure numpy is available
  #   "-I${python3.pkgs.numpy}/${python3.sitePackages}/numpy/core/include"
  #   # ???
  #   "-I${python3}/include/${python3.libPrefix}"
  # ];
  cmakeFlags = [
    "-DPYTHON_INCLUDE_DIR=${python3}/include/${python3.libPrefix}"
    "-DPYTHON_LIBPATH=${python3}/lib"
    "-DPYTHON_LIBRARY=${python3.libPrefix}"
    "-DPYTHON_NUMPY_INCLUDE_DIRS=${python311Packages.numpy}/${python3.sitePackages}/numpy/core/include"
    "-DPYTHON_NUMPY_PATH=${python311Packages.numpy}/${python3.sitePackages}"
    
    "-DPYTHON_VERSION=${python3.pythonVersion}"

    "-DWITH_PYTHON_INSTALL=OFF"
    "-DWITH_PYTHON_INSTALL_NUMPY=OFF"
    "-DWITH_PYTHON_INSTALL_REQUESTS=OFF"

    # wayland
    "-DWITH_GHOST_WAYLAND=ON"
    "-DWITH_GHOST_WAYLAND_DBUS=ON"
    "-DWITH_GHOST_WAYLAND_DYNLOAD=OFF"
    "-DWITH_GHOST_WAYLAND_LIBDECOR=ON"
  ];

  preConfigure = ''
    (
      expected_python_version=$(grep -E --only-matching 'set\(_PYTHON_VERSION_SUPPORTED [0-9.]+\)' build_files/cmake/Modules/FindPythonLibsUnix.cmake | grep -E --only-matching '[0-9.]+')
      actual_python_version=$(python -c 'import sys; print(".".join(map(str, sys.version_info[0:2])))')
      if ! [[ "$actual_python_version" = "$expected_python_version" ]]; then
        echo "wrong Python version, expected '$expected_python_version', got '$actual_python_version'" >&2
        exit 1
      fi
    )
  '';

  pythonPath =
    let
      ps = python311Packages;
    in
    [ps.numpy
     ps.requests
     ps.zstandard];

  #blenderExecutable = placeholder "out" + "/bin/blender";

  # /nix/store/b2lp2479725x61x2wg58x8ajgfz985w0-binutils-2.41/bin/ld.gold: error: cannot find -ltbb
  # /nix/store/b2lp2479725x61x2wg58x8ajgfz985w0-binutils-2.41/bin/ld.gold: error: cannot find -lshaderc_shared
  # /nix/store/b2lp2479725x61x2wg58x8ajgfz985w0-binutils-2.41/bin/ld.gold: error: cannot find -lvulkan
  # /nix/store/b2lp2479725x61x2wg58x8ajgfz985w0-binutils-2.41/bin/ld.gold: error: cannot find -ltbb
  
  # shellHook = ''
  #   export LD_LIBRARY_PATH="${lib.makeLibraryPath [
  #             wayland-scanner shaderc
  #             vulkan-headers
  #             vulkan-loader]}:$LD_LIBRARY_PATH"
  # '';
  lib-path = with pkgs;
    lib.makeLibraryPath [ libffi openssl python311Packages.numpy libdecor'];
  shellHook = ''
    #export out=/tmp/blender-test
    export NIX_ENFORCE_PURITY=0
    # Augment the dynamic linker path
    export "LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${lib-path}"
    export "LIBCLANG_PATH=${libclang.lib}/lib"
    cd ../build
    '';
  dontAddPrefix = true;
  configurePhase = ''
    cmake ../blender
  '';
  buildPhase = ''
    make -j24
  '';
  # installPhase = ''
  #   #mkdir -p $out/bin
  #   #mv blender $out/bin
  # '';
  # installPhase = ''
  #   make install
  # '';
  # installPhase = ''
  #   make DESTDIR=. install
  # '';
  # installPhase = ''
  #   make DESTDIR=/tmp/blender-test/ install
  # '';
  installPhase = ''
    make install DESTDIR=/tmp/blender-test/
  '';
  # installPhase = ''
  #   make install
  # '';

  # postInstall = ''
  #   cp $(DESTDIR)/$(prefix) $DESTDIR
  # '';

  # postInstall = ''
  #     mv $out/share/blender/${lib.versions.majorMinor finalAttrs.version}/python{,-ext}
  #   ''
  #   + ''
  #     buildPythonPath "$pythonPath"
  #     wrapProgram $blenderExecutable \
  #       --prefix PATH : $program_PATH \
  #       --prefix PYTHONPATH : "$program_PYTHONPATH" \
  #       --add-flags '--python-use-system-env'
  #   '';

  # is this needed? check later...
  # postInstall = ''
  #     buildPythonPath "$pythonPath"
  #     wrapProgram $blenderExecutable \
  #       --prefix PATH : $program_PATH \
  #       --prefix PYTHONPATH : "$program_PYTHONPATH" \
  #       --add-flags '--python-use-system-env'
  #   '';

  #export PYTHONPATH=${lib.makeSearchPath python.sitePackages [python311Packages.numpy]}:$PYTHONPATH
  postInstall = ''
      mv $out/share/blender/4.4/python{,-ext}
      wrapProgram $out/mnt/hdd/git/blender/fork/build/bin/blender \
        --prefix PATH : $program_PATH \
        --prefix PYTHONPATH : "${lib.makeSearchPath python.sitePackages [python311Packages.numpy]}" \
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
    
    #gcc14
    #gccStdenv 

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
    python311Packages.wrapPython

    #
    #gcc
    # glibc.static
    # glibc.out
    #glibc.out
    #glibc
    
    autoPatchelfHook

    openexr_3
    python311Packages.numpy

    libdecor'
    
  ];

  
  
  buildInputs = [
    llvmPackages.libcxxStdenv
    clang

    
    # build essentials
    #stdenv
    #gcc
    #coreutils
    
    
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
    #(opensubdiv.override { inherit cudaSupport; })
    #openvdb_11
    openvdb
    potrace
    pugixml
    
    python3
    # Include NumPy directly in Python environment
    #(python3.withPackages (ps: with ps; [ numpy ]))

    # python3 - originals
    # python311Packages.wrapPython
    # #python311Packages.materialx
    python311Packages.numpy
    # python311Packages.numpy
    # python311Packages.requests
    # python311Packages.zstandard

    
    
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

  
 

  # shellHook = ''
  #   export LD_LIBRARY_PATH="${lib.makeLibraryPath buildInputs}:$LD_LIBRARY_PATH"
  #   echo TRYING
  # '';
  #LD_LIBRARY_PATH = "${lib.makeLibraryPath buildInputs}:${LD_LIBRARY_PATH}";
  #LIBRARY_PATH = lib.makeLibraryPath [ libjpeg ];
  # cmakeFlags = ["-DCMAKE_MODULE_PATH=${libjpeg}"
  #               "-DLIBDIR=/does-not-exist"];
  #NIX_CFLAGS_LINK = [ "-ljpeg" ];
  # shellHook = ''
  #   export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}
  #   echo keklawl $out
  # '';
  #dontFixCmake = true;
  #doCheck = false;

  # cmakeFlags = [
  #   "-DCMAKE_INSTALL_PREFIX=$out"
  # ];

  #dontUseCmakeConfigure = true;
  # cmakeFlags = [
  #   "-DCMAKE_INSTALL_PREFIX=/mnt/hdd/git/blender-clone/build_linux_lite/bin/blender"
  # ];
  
  # blenderExecutable =
  #   placeholder "out"
  #   + "/bin/blender";

  # postInstall =
  # ''
  #   buildPythonPath "$pythonPath"
  #   wrapProgram $blenderExecutable \
  #     --prefix PATH : $program_PATH \
  #     --prefix PYTHONPATH : "$program_PYTHONPATH" \
  #     --add-flags '--python-use-system-env'
  # '';

  

  #name = "my-blender";
  #name = "blender";
 # shellHook = ''
  #   echo keklawl $out
  #   make lite
  # '';
  #dontFixCmake = true;
  #dontAddPrefix = true;
  
  #src = ./.;
  #src = ../build_linux_lite/bin/blender;
  #mainProgram = "blender";
  # buildPhase = ''
  #   make lite
  # '';

  # blenderExecutable = "/home/travis/temporary/test/blender";
  # #postInstall = "mv /home/travis/temporary/test/blender";
  # preInstall = "NIX pre install BEGIN";
  # postInstall = "NIX post install BEGIN";
  # mainProgram = "blender";
}
