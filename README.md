# CoSense
Cosense is a part of the [Noisy&Newton Project](https://github.com/phillipstanleymarbell/Noisy-lang-compiler). Please check it for more details.
The main idea of CoSense is to use sensor specifications to help compilation optimization, including value range propagation, function overload & elimination, type compression, condition simplification, and constant substitution.
- - - -

## Getting started

The correct way to clone this repository to get the submodules is:
```bash
	git clone --recursive git@github.com:systems-nuts/CoSense.git
```

To update all submodules:
```bash
	git pull --recurse-submodules
	git submodule update --remote --recursive
```

If you forgot to clone with `--recursive` and end up with empty submodule directories, you can remedy this with
```bash
	git submodule update --init
```

Building the Noisy compiler and debug tools depends on the [`libflex`](https://github.com/phillipstanleymarbell/libflex), [`Wirth-tools`](https://github.com/phillipstanleymarbell/Wirth-tools), and [`DTrace-scripts`](https://github.com/phillipstanleymarbell/DTrace-scripts) repositories. These repositories are already included as submodules:
```
	Libflex:		git@github.com:phillipstanleymarbell/libflex.git
	Wirth tools:		git@github.com:phillipstanleymarbell/Wirth-tools.git
	DTrace-scripts:		git@github.com:phillipstanleymarbell/DTrace-scripts.git
```

For linear algebra in Newton, we use the Eigen library. This is also already linked to the repository as a submodule:
```
	Eigen:			git@github.com:eigenteam/eigen-git-mirror.git	
```

The build also depends on the C protobuf compiler, `sloccount`, and on Graphviz. 
```bash
sudo apt install libprotobuf-c-dev protobuf-c-compiler sloccount graphviz-devel
```

Furthermore, LLVM is a build and runtime dependency on this project.
Currently, passes related to LLVM are tested with LLVM 13 versions.

Make sure `llvm-config` is installed for one of the above versions. In case it is named differently, e.g., `llvm-config-x` you will need to create a symbolic link:

```bash
cd /location/of/llvm-config-x
ln -s llvm-config-x llvm-config
```

Once you have the above repositories, 

1.	Create a file `config.local` in the root of the Noisy tree and edit it to contain 
```make
	LIBFLEXPATH     = full-path-to-libflex-repository-clone
	CONFIGPATH      = full-path-to-libflex-repository-clone
	OSTYPE		= linux
	MACHTYPE	= x86_64
```

For example,
```make
	LIBFLEXPATH=/home/me/Noisy-lang-compiler/submodules/libflex
	CONFIGPATH=/home/me/Noisy-lang-compiler/submodules/libflex
	OSTYPE		= linux
	MACHTYPE	= x86_64
```

2.	Copy `config.local` to the libflex directory
```shell
	$ cp config.local submodules/libflex
```

3.	In `src/common/Makefile` and `src/newton/Makefile`, change `COMPILERVARIANT` as necessary (default is `clang`).


4.	Build Libflex by going to the directory you cloned for Libflex and 
running `make`. The Makefile assumes the environment variables `OSTYPE`
and `MACHTYPE` are set. If that is not the case, you will need to 
explicitly set them, for example on macOS:
```shell
$ cd submodules/libflex
$ make
```

5.	From the root of this top-level repository, build the Noisy and Newton compilers by running `make`. The makefile assumes the  environment variables `OSTYPE` and `MACHTYPE` are set. If that is not the  case, you will need to explicitly set them, for example on macOS:
```shell
$ make -j32
```

