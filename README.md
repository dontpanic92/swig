## Alternative Go Backend For SWIG

[![Build Status](https://travis-ci.org/dontpanic92/swig.svg?branch=new-gobackend)](https://travis-ci.org/dontpanic92/swig)

This repo is a demo of an alternative Go backend for SWIG. It uses the anonymous field feature in golang to shorten the wrapper code, and it also introduced a new feature to let Go's GC track C++ objects.

For one of my projects [wxGo](https://github.com/dontpanic92/wxGo), there is a noticable improvement on the wrapper size. The `.go` file is about 30% smaller (declined from 321317 lines to 218316 lines, from ~12MB to ~8MB), the `.cxx` file is about 50% smaller (from 465027 lines to 220700 lines, from ~12MB to ~6MB). 

The executable that use the library are also smaller. They are 5MB ~ 15MB smaller than before (both stripped), depending on the code.

Only working with `-cgo` option.

### Let Go's GC Track C++ Objects

#### Current Memory Management

For now, SWIG generated go code will not track the C++ object. That means we have to manually call destructors if we won't use any C++ object. For example,

```
// Go code
import "wrap"

func test() {
    obj := wrap.NewObject()

    size := wrap.NewSize(40, 40)
    obj.SetWindowSize(size)
    wrap.DeleteSize(size)

    wrap.DeleteObject(obj)
}
```  

#### New SWIG option `-trackobject N`

`-trackobject` can generate some useful functions and code to let Go's garbage collector delete C++ objects automatically when we don't need them. It has 4 levels for now, i.e `N` can be 0, 1, 2, or 3:

- Level 0: Do nothing.
- Level 1: Add `SwigTrackObject` and `SwigUntrackObject` functions to every object. Users can call these functions to let Go's GC track the object.
- Level 2: Automatically track the objects that is returned by a function by value. For example the object of `A` returned by `SomeFunction`,
```
struct A {/*...*/};
A SomeFunction(){ return A{}; }
```
These objects cannot be taken own by other functions, so we can safely track it.
- Level 3: (Experimental!!) Automatically track every object when it is created.

Using `-trackobject 3` option, we can write the following code without memory leak:

```
// Go code
import "wrap"

func test() {
    obj := wrap.NewObject()
    obj.SetWindowSize(wrap.NewSize(40, 40))
}
```

However, we must think more if we want to use `-trackobject 3` option. There are some issues about it.

#### Ownership Issues

It should be noticed that some C++ functions/classes will free or take ownship of other objects. That is, the objects will be deleted by C++ code. For example, there is a C++ function and a class that make things a bit complicated:

```
// C++ code
void DoSomething(Object* obj) {
    delete obj; // Oops!
}

class SomeClass {
public:
    void Attach(Object* obj) { mObj = obj; }
    ~SomeClass() { delete mObj; } // Oops!
private:
    Object* mObj;
};

```

If we use `-trackobject 3` option, we must let Go know about it. Otherwise the obejcts will be double freed. This can be done by change the parameters' names to `SWIG_TAKEOWN`, after including `trackobjects.i`. SWIG source code is as below:

```
// SWIG code
%include "trackobjects.i"

void DoSomething(Object* SWIG_TAKEOWN);

class SomeClass {
public:
    void Attach(Object* SWIG_TAKEOWN);
};

```

Then Go will stop tracking the object when these methods are called. How about `Detach`? Some detach-like functions will receive an object as parameters to find which object to detach. For example,

```
// C++ code
void SomeClass::Detach(Object* obj) {
    mObjects.remove(obj);
}
```

For these functions, we can change their names to `SWIG_DROPOWN` to let Go track them again:

```
// SWIG code
void SomeClass::Detach(Object* SWIG_DROPOWN);
```

If detach functions don't receive a parameter, we can only use `obj.SwigTrackObject()` in Go to track it again, or remember to use `wrap.DeleteObject(obj)` to manually delete it.

### Not tracking specific classes

For some reason, we may not want to track some specific C++ classes' objects. We can use `%notracking` directive to achieve this:

```
// SWIG code
%include "trackobjects.i"

%notracking SomeClass;
class SomeClass {};
```
