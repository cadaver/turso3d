flextGL
=======

flextGL is an OpenGL extension loader generator.

It is a bit different than other comparable systems:

 * Gives complete control over exposed version and extensions
 * (Optionally) exports only core-profile functions
 * Only requested extensions are loaded
 * Bindings directly generated from the OpenGL registry's `gl.xml` file
 * Flexible python template system for source-generation
 * Source templates easy to adapt to project requirements

You will need the following dependencies:

 * [Python 3.x](http://python.org)
 * [Wheezy Template](http://packages.python.org/wheezy.template)

### What's new?

* **August 7 2014:** Adding support for generating OpenGL ES loading code
  
* **July 10 2014:** flextGL now parses `gl.xml` instead of the deprecated `.spec` files.


Source tree
-----------

* `flextGLgen.py`
  > The generator script.

* `flext.py`
  > The parsing code
  
* `templates/`
  > The  sub-directories  in here  contain the different  template sets.
  > You can  add your own  template by simply  creating a new  folder in 
  > there.

* `profiles/`
  > Some example profile files to give  you an idea on how to write your 
  > own. 'profiles/exampleProfile.txt' contains a lot of comments to get
  > you up to speed.
   
* `spec/` (generated)
  > This directory is  automatically created by the script  to store the
  > downloaded OpenGL .spec files.


Usage
-----

You create your  loader code by writing a profile  file and passing it
to the script.

Here is what a typical profile might look like:

    version 3.3 core
    extension EXT_texture_filter_anisotropic optional
    extension ARB_tesselation_shader optional

This  requests   an  OpenGL  core  profile  and   the  extensions  for
anisotropic filtering and  tesselation shaders. Those extensions  were
requested  as 'optional'. This  means that  a missing  extension won't
cause  an error.  Instead, the  programmer will  have to  check before
using  it. This  can  be easily  done  by testing  a generated  global
variable. For OpenGL ES a typical profile might look like:

    version 3.0 es
    extension OES_standard_derivatives optional
    extension OES_vertex_array_object optional

The profile file is then passed to the script like this:

    $ python flextGLgen.py -D generated profile.txt

This  will  create  the  requested  source  code and  put  it  in  the
'generated' directory.

The  best  way  to work  with  flextGL  is  to  integrate it  in  your
build-system.
The example project demonstrates this for Make and CMake [here](https://github.com/ginkgo/flextGL-example).


Generated API
-------------

The generated API boils down to a few things:

* `int flextInit()`
  > Initializes the OpenGL functions after context creation. 

* `FLEXT_MAJOR_VERSION`
  > The OpenGL major version defined in the profile file.

* `FLEXT_MINOR_VERSION`
  > The OpenGL minor version defined in the profile file.

* `FLEXT_CORE_PROFILE`
  > Boolean variable.  Is GL_TRUE,  if the profile  file defined  a core
  > profile. 

* `FLEXT_<extension-name>`
  > Generated global  variables for checking if a  specific extension is
  > supported. 

Take a look at the example program to get an idea on how it's used.

Templates
---------

At the moment, there are three template sets available:

* `'compatible'`
  > This loads the extensions using a framework-agnostic method with WGL
  > AGL or GLX. This  is  probably the sensible default for most people.
  > It has not been thoroughly tested yet, though.

* `'glfw'`
  > This  uses  GLFW 2's functions  for  loading  and  testing for  OpenGL
  > extensions.  It will  obviously only  work  with GLFW,  but is  well
  > tested and the generated source code is very easy to understand.

* `'glfw3'`
  > This  works like the `glfw` template, but uses GLFW 3 instead. In this
  > template, a pointer to the GLFWwindow has to be passed as a parameter
  > of `flextInit()`.

* `'glfw3-es'`
  > Used for generating OpenGL ES loading code.

  
Installing Wheezy Template on Windows
-------------------------------------

If you have Python 3.4+ installed you should be able to install Wheezy using pip.

    $ pip install --user wheezy.template

The `--user` does a local install in your home-folder. You can omit it if you want
to do a system-wide installation.

If you have an older version of Python or don't have pip for some reason, then
you need to [install](https://pip.pypa.io/en/latest/installing.html) it first.


Copyright
---------

The  "compatible"  template uses  a few  code  snippets  from Slavomir
Kaslev's  gl3w  OpenGL core  profile loader  for  portable  loading of
procedures and checking minor/major version in OpenGL < 3.0.

Mykhailo Parfeniuk([sopyer](https://github.com/sopyer)) provided most of the `gl.xml` parsing code.

Vladimír Vondruš([mosra](https://github.com/mosra)) added support for OpenGL ES loader generation.

    (C) Thomas Weber, 2011-2014
        ginko (at) cg (dot) tuwien (dot) ac (dot) at
        
