## Alternative Go Backend For Swig

[![Build Status](https://travis-ci.org/dontpanic92/swig.svg?branch=new-gobackend)](https://travis-ci.org/dontpanic92/swig)

This repo is a demo of an alternative go backend for swig. It uses the anonymous field feature in golang to shorten the wrapper code. 

For one of my projects [wxGo](https://github.com/dontpanic92/wxGo), there is a noticable improvement on the wrapper size. The `.go` file is about 30% smaller (declined from 321317 lines to 218316 lines, from ~12MB to ~8MB), the `.cxx` file is about 50% smaller (from 465027 lines to 220700 lines, from ~12MB to ~6MB). 

The executable that use the library are also smaller. They are 5MB ~ 15MB smaller than before, depending on the code.

Only working with `-cgo` option.