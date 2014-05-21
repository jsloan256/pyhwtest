#!/usr/bin/env python
# vim: ai ts=4 sts=4 et sw=4
from distutils.core import setup, Extension
setup(name='hwtest', 
      version="0.1", 
      py_modules=['hwtest',],
      ext_modules=[Extension('chwtest', ['chwtest.c'])],
      )
