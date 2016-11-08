
%typemap(goin) SWIGTYPE *SWIG_TAKEOWN, SWIGTYPE &SWIG_TAKEOWN %{
    $input.SwigUntrackObject()
    $result = $input
%}

%typemap(goargout) SWIGTYPE *SWIG_DROPOWN, SWIGTYPE &SWIG_DROPOWN %{
    $input.SwigTrackObject()
%}

#define %notracking %feature("notracking", "1")
#define %clearnotracking %feature("notracking","")
#define %trackself %feature("trackself", "1")
#define %untrackself %feature("trackself", "0")
#define %cleartrackself %feature("trackself", "")
