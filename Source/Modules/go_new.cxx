
class GO_New: public GO {
protected:
    bool convertFunction;
public:
    GO_New(): convertFunction(false) {}

    /* ----------------------------------------------------
     * Override the original class handler. We will utilize
     * the anonymous field feature in go to make the wrapper 
     * code shorter. 
     * ---------------------------------------------------- */

    int classHandler(Node *n) {
        int ret = 0;
        class_node = n;
        List *baselist = Getattr(n, "bases");
        bool has_base_classes = baselist && Len(baselist) > 0;    
        String *name = Getattr(n, "sym:name");
        String *go_name = exportedName(name);


        if (!checkNameConflict(go_name, n, NULL)) {
            Delete(go_name);
            SetFlag(n, "go:conflict");
            return SWIG_NOWRAP;
        }

        String *go_class_name = goCPointerType(Getattr(n, "classtypeobj"), true);
        String *go_swigcptr_name = NewString("Swigcptr");
        Append(go_swigcptr_name, go_name);

        class_name = name;
        class_receiver = go_class_name;
        class_methods = NewHash();

        int isdir = GetFlag(n, "feature:director");
        int isnodir = GetFlag(n, "feature:nodirector");
        bool is_director = isdir && !isnodir;

        Printv(f_go_wrappers, "type ", go_class_name, " struct {\n", NULL);
        Printv(f_go_wrappers, "\t", go_swigcptr_name, " uintptr\n", NULL);
        if (has_base_classes) {
            for (Iterator b = First(baselist); b.item; b = Next(b)) {
                Printv(f_go_wrappers, "\tSwigClass", Getattr(b.item, "classtype"), "\n", NULL);
            }
        }
        Printv(f_go_wrappers, "}\n\n", NULL);

        // A method to return the pointer to the C++ class.  This is used
        // by generated code to convert between the interface and the C++
        // value.
        Printv(f_go_wrappers, "func (p ", go_class_name, ") Swigcptr() uintptr {\n", NULL);
        Printv(f_go_wrappers, "\treturn p.", go_swigcptr_name,"\n", NULL);
        Printv(f_go_wrappers, "}\n\n", NULL);

        Printv(f_go_wrappers, "func ", go_class_name, "_setSwigcptr(ptr uintptr) *", go_class_name," {\n", NULL);
        Printv(f_go_wrappers, "\tp := &", go_class_name, "{}\n", NULL);
        Printv(f_go_wrappers, "\tp.", go_swigcptr_name, " = ptr\n", NULL);
        if (has_base_classes) {
            int i = 0;
            for (Iterator b = First(baselist); b.item; b = Next(b)) {
                String *go_base_class_name = NewString("SwigClass");
                String *go_base_exported_name = exportedName(Getattr(b.item, "classtype"));
                Append(go_base_class_name, go_base_exported_name);
                if (i == 0) {
                    Printv(f_go_wrappers, "\tp.", go_base_class_name, ".Swigcptr", go_base_exported_name, " = ptr\n", NULL);
                } else {
                    Printv(f_go_wrappers, "\tp.", go_base_class_name, " = p.swigGet", go_base_class_name, "().(", go_base_class_name,")\n", NULL);
                }
                Delete(go_base_class_name);
                Delete(go_base_exported_name);
                i++;
            }
        }
        Printv(f_go_wrappers, "\treturn p\n}\n\n", NULL);

        Printv(f_go_wrappers, "func (p ", go_class_name, ") _swigIs", go_class_name, "() {\n", NULL);
        Printv(f_go_wrappers, "}\n\n", NULL);

        if (is_director) {
            // Return the interface passed to the NewDirector function.
            Printv(f_go_wrappers, "func (p ", go_swigcptr_name, ") DirectorInterface() interface{} {\n", NULL);
            Printv(f_go_wrappers, "\treturn nil\n", NULL);
            Printv(f_go_wrappers, "}\n\n", NULL);
        }

        // We have seen a definition for this type.
        Setattr(defined_types, go_name, go_name);
        //Setattr(defined_types, go_type_name, go_type_name); 
        Setattr(defined_types, go_class_name, go_class_name);

        interfaces = NewString("");
        ret = Language::classHandler(n);
        if (ret != SWIG_OK) {
            goto CleanUp;
        }

        if (has_base_classes) {
            Hash *parents = NewHash();
            addConvertFunctions(n, parents, baselist);
            ret = addSwigConvertFunctions(n, parents, baselist);
            //addFirstBaseInterface(n, parents, baselist);
            //int r = addExtraBaseInterfaces(n, parents, baselist);
            Delete(parents);
        }

        // Generate interface
        Printv(f_go_wrappers, "type ", go_name, " interface {\n", NULL);
        Printv(f_go_wrappers, "\tSwigcptr() uintptr\n", NULL);
        Printv(f_go_wrappers, "\t_swigIs", go_class_name, "()\n", NULL);

        if (is_director) {
            Printv(f_go_wrappers, "\tDirectorInterface() interface{}\n", NULL);
        }
        Append(f_go_wrappers, interfaces);
        Printv(f_go_wrappers, "}\n\n", NULL);
        ret = SWIG_OK;
CleanUp:
        Delete(go_name);
        Delete(go_swigcptr_name);
        Delete(go_class_name);
        Delete(interfaces);
        interfaces = NULL;
        class_name = NULL;
        class_receiver = NULL;
        class_node = NULL;
        class_methods = NULL;
        Delete(class_methods);
        return ret;
    }

    String* goCPointerType(SwigType *type, bool add_to_hash) {
        SwigType *ty = SwigType_typedef_resolve_all(type);
        Node *cn = classLookup(ty);
        String *ex;
        String *ret;
        if (!cn) {
            if (add_to_hash) {
                Setattr(undefined_types, ty, ty);
            }
            ret = NewString("SwigClass");
            ex = exportedName(ty);
            Append(ret, ex);
        } else {
            String *cname = Getattr(cn, "sym:name");
            if (!cname) {
                cname = Getattr(cn, "name");
            }
            ex = exportedName(cname);
            Node *cnmod = Getattr(cn, "module");
            if (!cnmod || Strcmp(Getattr(cnmod, "name"), module) == 0) {
                if (add_to_hash) {
                    Setattr(undefined_types, ty, ty);
                }
                ret = NewString("SwigClass");
                Append(ret, ex);
            } else {
                ret = NewString("");
                Printv(ret, getModuleName(Getattr(cnmod, "name")), ".SwigClass", ex, NULL);
            }
        }
        Delete(ty);
        Delete(ex);
        return ret;
    }

    int cgoGoWrapper(const cgoWrapperInfo *info) {

        Wrapper *dummy = initGoTypemaps(info->parms);

        bool add_to_interface = interfaces && !info->is_constructor && !info->is_destructor && !info->is_static && !info->overname && checkFunctionVisibility(info->n, NULL);

        Printv(f_go_wrappers, "func ", NULL);

        Parm *p = info->parms;
        int pi = 0;

        // Add the receiver first if this is a method.
        if (info->receiver) {
            Printv(f_go_wrappers, "(", NULL);
            if (info->base && info->receiver) {
                Printv(f_go_wrappers, "_swig_base", NULL);
            } else {
                Printv(f_go_wrappers, Getattr(p, "lname"), NULL);
                p = nextParm(p);
                ++pi;
            }
            Printv(f_go_wrappers, " ", info->receiver, ") ", NULL);
        }

        Printv(f_go_wrappers, info->go_name, NULL);
        if (info->overname) {
            Printv(f_go_wrappers, info->overname, NULL);
        }
        Printv(f_go_wrappers, "(", NULL);

        // If we are doing methods, add this method to the interface.
        if (add_to_interface) {
            Printv(interfaces, "\t", info->go_name, "(", NULL);
        }

        // Write out the parameters to both the function definition and
        // the interface.

        String *parm_print = NewString("");

        int parm_count = emit_num_arguments(info->parms);
        int required_count = emit_num_required(info->parms);
        int args = 0;

        for (; pi < parm_count; ++pi) {
            p = getParm(p);
            if (pi == 0 && info->is_destructor) {
                String *cl = exportedName(class_name);
                Printv(parm_print, Getattr(p, "lname"), " ", cl, NULL);
                Delete(cl);
                ++args;
            } else {
                if (args > 0) {
                    Printv(parm_print, ", ", NULL);
                }
                ++args;
                if (pi >= required_count) {
                    Printv(parm_print, "_swig_args ...interface{}", NULL);
                    break;
                }
                Printv(parm_print, Getattr(p, "lname"), " ", NULL);
                String *tm = goType(p, Getattr(p, "type"));
                Printv(parm_print, tm, NULL);
                Delete(tm);
            }
            p = nextParm(p);
        }

        Printv(parm_print, ")", NULL);

        // Write out the result type.
        if (info->is_constructor) {
            String *cl = goCPointerType(class_name, false);
            Printv(parm_print, " (_swig_ret *", cl, ")", NULL);
            Delete(cl);
        } else {
            if (SwigType_type(info->result) != T_VOID) {
                String *tm = goType(info->n, info->result);
                if (!goTypeIsInterface(info->n, info->result)) {
                    Printv(parm_print, " (_swig_ret ", tm, ")", NULL);
                } else {
                    Printv(parm_print, " (_swig_ret *SwigClass", tm, ")", NULL);
                }
                Delete(tm);
            }
        }

        Printv(f_go_wrappers, parm_print, NULL);
        if (add_to_interface) {
            Printv(interfaces, parm_print, "\n", NULL);
        }

        // Write out the function body.

        Printv(f_go_wrappers, " {\n", NULL);

        if (parm_count > required_count) {
            Parm *p = info->parms;
            int i;
            for (i = 0; i < required_count; ++i) {
                p = getParm(p);
                p = nextParm(p);
            }
            for (; i < parm_count; ++i) {
                p = getParm(p);
                String *tm = goType(p, Getattr(p, "type"));
                Printv(f_go_wrappers, "\tvar ", Getattr(p, "lname"), " ", tm, "\n", NULL);
                Printf(f_go_wrappers, "\tif len(_swig_args) > %d {\n", i - required_count);
                Printf(f_go_wrappers, "\t\t%s = _swig_args[%d].(%s)\n", Getattr(p, "lname"), i - required_count, tm);
                Printv(f_go_wrappers, "\t}\n", NULL);
                Delete(tm);
                p = nextParm(p);
            }
        }

        String *call = NewString("\t");

        String *ret_type = NULL;
        bool memcpy_ret = false;
        String *wt = NULL;
        if (SwigType_type(info->result) != T_VOID) {
            if (info->is_constructor) {
                ret_type = NewString("*SwigClass");
                Append(ret_type, class_name);
            } else {
                ret_type = goImType(info->n, info->result);
                if (goTypeIsInterface(info->n, info->result)) {
                    String *tmp = NewString("*SwigClass");
                    Append(tmp, ret_type);
                    Delete(ret_type);
                    ret_type = tmp;
                }
            }
            Printv(f_go_wrappers, "\tvar swig_r ", ret_type, "\n", NULL);

            bool c_struct_type;
            Delete(cgoTypeForGoValue(info->n, info->result, &c_struct_type));
            if (c_struct_type) {
                memcpy_ret = true;
            }

            if (memcpy_ret) {
                Printv(call, "swig_r_p := ", NULL);
            } else {
                Printv(call, "swig_r = (", ret_type, ")(", NULL);
            }

            if (info->is_constructor || goTypeIsInterface(info->n, info->result) || convertFunction) {
                wt = goWrapperType(info->n, info->result, true);
                //if (info->is_constructor || convertFunction) {
                Printv(call, wt, "_setSwigcptr(uintptr(", NULL);
                /*} else {
                    Printv(call, "(unsafe.Pointer)((uintptr)(", NULL);
                }*/
            }
        }

        Printv(call, "C.", info->wname, "(", NULL);

        args = 0;

        if (parm_count > required_count) {
            Printv(call, "C.swig_intgo(len(_swig_args))", NULL);
            ++args;
        }

        if (info->base && info->receiver) {
            if (args > 0) {
                Printv(call, ", ", NULL);
            }
            ++args;
            Printv(call, "C.uintptr_t(_swig_base)", NULL);
        }

        p = info->parms;
        for (int i = 0; i < parm_count; ++i) {
            p = getParm(p);
            if (args > 0) {
                Printv(call, ", ", NULL);
            }
            ++args;

            SwigType *pt = Getattr(p, "type");
            String *ln = Getattr(p, "lname");

            String *ivar = NewStringf("_swig_i_%d", i);

            String *goin = goGetattr(p, "tmap:goin");
            if (goin == NULL) {
                Printv(f_go_wrappers, "\t", ivar, " := ", ln, NULL);
                if ((i == 0 && info->is_destructor) || ((i > 0 || !info->receiver || info->base || info->is_constructor) && goTypeIsInterface(p, pt)) || (i == 0 && info->receiver)) {
                    Printv(f_go_wrappers, ".Swigcptr()", NULL);
                }
                Printv(f_go_wrappers, "\n", NULL);
                Setattr(p, "emit:goinput", ln);
            } else {
                String *itm = goImType(p, pt);
                Printv(f_go_wrappers, "\tvar ", ivar, " ", itm, "\n", NULL);
                goin = Copy(goin);
                Replaceall(goin, "$input", ln);
                Replaceall(goin, "$result", ivar);
                Printv(f_go_wrappers, goin, "\n", NULL);
                Delete(goin);
                Setattr(p, "emit:goinput", ivar);
            }

            bool c_struct_type;
            String *ct = cgoTypeForGoValue(p, pt, &c_struct_type);
            if (c_struct_type) {
                Printv(call, "*(*C.", ct, ")(unsafe.Pointer(&", ivar, "))", NULL);
            } else {
                Printv(call, "C.", ct, "(", ivar, ")", NULL);
            }
            Delete(ct);

            p = nextParm(p);
        }

        Printv(f_go_wrappers, call, ")", NULL);
        Delete(call);

        if (wt) {
            // Close the type conversion to the wrapper type.
            Printv(f_go_wrappers, "))", NULL);
        }
        /*if (convertFunction || info->is_constructor) {
            Printv(f_go_wrappers, ")", NULL);
        }*/
        if (SwigType_type(info->result) != T_VOID && !memcpy_ret) {
            // Close the type conversion of the return value.
            Printv(f_go_wrappers, ")", NULL);
        }

        Printv(f_go_wrappers, "\n", NULL);

        if (memcpy_ret) {
            Printv(f_go_wrappers, "\tswig_r = *(*", ret_type, ")(unsafe.Pointer(&swig_r_p))\n", NULL);
        }
        
        if (ret_type) {
            Delete(ret_type);
        }

        goargout(info->parms);

        if (SwigType_type(info->result) != T_VOID) {
            Swig_save("cgoGoWrapper", info->n, "type", "tmap:goout", NULL);
            Setattr(info->n, "type", info->result);

            String *goout = goTypemapLookup("goout", info->n, "swig_r");
            if (goout == NULL) {
                Printv(f_go_wrappers, "\treturn swig_r\n", NULL);
            } else {
                String *tm = goType(info->n, info->result);
                Printv(f_go_wrappers, "\tvar swig_r_1 ", tm, "\n", NULL);
                goout = Copy(goout);
                Replaceall(goout, "$input", "swig_r");
                Replaceall(goout, "$result", "swig_r_1");
                Printv(f_go_wrappers, goout, "\n", NULL);
                Printv(f_go_wrappers, "\treturn swig_r_1\n", NULL);
            }

            Swig_restore(info->n);
        }

        Printv(f_go_wrappers, "}\n\n", NULL);

        DelWrapper(dummy);

        return SWIG_OK;
    }

    void addConvertFunctions(Node *n, Hash *parents, List *bases) {
        if (!bases || Len(bases) == 0) {
            return;
        }
        
        for (Iterator b = First(bases); b.item; b = Next(b)) {
            if (!GetFlag(b.item, "feature:ignore")) {
                String *go_name = buildGoName(Getattr(n, "sym:name"), false, false);
                String *go_type_name = goCPointerType(Getattr(n, "classtypeobj"), true);
                String *go_base_name = exportedName(Getattr(b.item, "sym:name"));
                String *go_base_type = goType(n, Getattr(b.item, "classtypeobj"));
                String *go_base_type_name = goCPointerType(Getattr(b.item, "classtypeobj"), true);
                String *go_class_name = NewString("SwigClass");
                String *go_base_class_name = NewString("SwigClass");
                Append(go_class_name, go_name);
                Append(go_base_class_name, go_base_name);

                Printv(f_go_wrappers, "func (p ", go_class_name, ") SwigIs", go_base_class_name, "() {\n", NULL);
                Printv(f_go_wrappers, "}\n\n", NULL);


                Printv(f_go_wrappers, "func (p ", go_class_name, ") To", go_base_name, "() ", go_base_class_name, " {\n", NULL);
                Printv(f_go_wrappers, "\treturn p.", go_base_class_name, "\n", NULL);
                Printv(f_go_wrappers, "}\n\n", NULL);

                Setattr(parents, go_base_name, NewString(""));

                Delete(go_name);
                Delete(go_type_name);
                Delete(go_base_type);
                Delete(go_base_type_name);
                Delete(go_class_name);
                Delete(go_base_class_name);
            }
            addConvertFunctions(n, parents, Getattr(b.item, "bases"));
        }
    }

    int addSwigConvertFunctions(Node *n, Hash *parents, List *bases) {
        Iterator b = First(bases);

        for (b = Next(b); b.item; b = Next(b)) {
            if (GetFlag(b.item, "feature:ignore")) {
                continue;
            }

            String *go_base_name = exportedName(Getattr(b.item, "sym:name"));

            Swig_save("addExtraBaseInterface", n, "wrap:action", "wrap:name", "wrap:parms", NULL);

            SwigType *type = Copy(Getattr(n, "classtypeobj"));
            SwigType_add_pointer(type);
            Parm *parm = NewParm(type, "self", n);
            Setattr(n, "wrap:parms", parm);

            String *pn = Swig_cparm_name(parm, 0);
            String *action = NewString("");
            Printv(action, Swig_cresult_name(), " = (", Getattr(b.item, "classtype"), "*)", pn, ";", NULL);
            Delete(pn);

            Setattr(n, "wrap:action", action);

            String *name = Copy(class_name);
            Append(name, "_SwigGet");
            Append(name, go_base_name);

            String *go_name = NewString("swigGetSwigClass");
            String *c1 = exportedName(go_base_name);
            Append(go_name, c1);
            Delete(c1);

            String *wname = Swig_name_wrapper(name);
            Append(wname, unique_id);
            Setattr(n, "wrap:name", wname);

            SwigType *result = Copy(Getattr(b.item, "classtypeobj"));
            SwigType_add_pointer(result);
            convertFunction = true;
            int r = makeWrappers(n, name, go_name, NULL, wname, NULL, parm, result, false);
            convertFunction = false;
            if (r != SWIG_OK) {
                return r;
            }

            Swig_restore(n);

            Setattr(parents, go_base_name, NewString(""));

            Delete(go_name);
            Delete(type);
            Delete(parm);
            Delete(action);
            Delete(result);

            addSwigConvertFunctions(n, parents, Getattr(b.item, "bases"));
        }
        return SWIG_OK;
    }

    String *goWrapperType(Node *n, SwigType *type, bool is_result) {
        bool is_interface;
        String *ret = goTypeWithInfo(n, type, true, &is_interface);

        // If this is an interface, we want to pass the real type.
        if (is_interface) {
            Delete(ret);
            if (!is_result) {
                ret = NewString("uintptr");
            } else {
                SwigType *ty = SwigType_typedef_resolve_all(type);
                while (true) {
                    if (SwigType_ispointer(ty)) {
                        SwigType_del_pointer(ty);
                    } else if (SwigType_isarray(ty)) {
                        SwigType_del_array(ty);
                    } else if (SwigType_isreference(ty)) {
                        SwigType_del_reference(ty);
                    } else if (SwigType_isqualifier(ty)) {
                        SwigType_del_qualifier(ty);
                    } else {
                        break;
                    }
                }
                assert(SwigType_issimple(ty));
                String *p = goCPointerType(ty, true);
                Delete(ty);
                ret = p;
            }
        }

        return ret;
    }

    int classDirectorConstructor(Node *n) {
        bool is_ignored = GetFlag(n, "feature:ignore") ? true : false;

        String *name = Getattr(n, "sym:name");
        if (!name) {
            assert(is_ignored);
            name = Getattr(n, "name");
        }

        String *overname = NULL;
        if (Getattr(n, "sym:overloaded")) {
            overname = Getattr(n, "sym:overname");
        }

        String *go_name = exportedName(name);

        ParmList *parms = Getattr(n, "parms");
        Setattr(n, "wrap:parms", parms);

        String *cn = exportedName(Getattr(parentNode(n), "sym:name"));

        String *go_type_name = goCPointerType(Getattr(parentNode(n), "classtypeobj"), true);

        String *director_struct_name = NewString("_swig_Director");
        Append(director_struct_name, cn);

        String *fn_name = NewString("_swig_NewDirector");
        Append(fn_name, cn);
        Append(fn_name, go_name);

        if (!overname && !is_ignored) {
            if (!checkNameConflict(fn_name, n, NULL)) {
                return SWIG_NOWRAP;
            }
        }

        String *fn_with_over_name = Copy(fn_name);
        if (overname) {
            Append(fn_with_over_name, overname);
        }

        String *wname = Swig_name_wrapper(fn_name);

        if (overname) {
            Append(wname, overname);
        }
        Append(wname, unique_id);
        Setattr(n, "wrap:name", wname);

        bool is_static = isStatic(n);

        Wrapper *dummy = NewWrapper();
        emit_attach_parmmaps(parms, dummy);
        DelWrapper(dummy);

        Swig_typemap_attach_parms("gotype", parms, NULL);
        Swig_typemap_attach_parms("goin", parms, NULL);
        Swig_typemap_attach_parms("goargout", parms, NULL);
        Swig_typemap_attach_parms("imtype", parms, NULL);
        int parm_count = emit_num_arguments(parms);

        String *func_name = NewString("NewDirector");
        Append(func_name, go_name);

        String *func_with_over_name = Copy(func_name);
        if (overname) {
            Append(func_with_over_name, overname);
        }

        SwigType *first_type = NewString("int");
        Parm *first_parm = NewParm(first_type, "swig_p", n);
        set_nextSibling(first_parm, parms);
        Setattr(first_parm, "lname", "p");

        Parm *p = parms;
        for (int i = 0; i < parm_count; ++i) {
            p = getParm(p);
            Swig_cparm_name(p, i);
            p = nextParm(p);
        }

        if (!is_ignored) {
            if (cgo_flag) {
                Printv(f_cgo_comment, "extern uintptr_t ", wname, "(int", NULL);

                p = parms;
                for (int i = 0; i < parm_count; ++i) {
                p = getParm(p);
                bool c_struct_type;
                String *ct = cgoTypeForGoValue(p, Getattr(p, "type"), &c_struct_type);
                Printv(f_cgo_comment, ", ", ct, " ", Getattr(p, "lname"), NULL);
                p = nextParm(p);
                }
                Printv(f_cgo_comment, ");\n", NULL);
            } else {
                // Declare the C++ wrapper.

                if (!gccgo_flag) {
                    Printv(f_go_wrappers, "var ", wname, " unsafe.Pointer\n\n", NULL);
                } else {
                    Printv(f_go_wrappers, "//extern ", go_prefix, "_", wname, "\n", NULL);
                }

                Printv(f_go_wrappers, "func ", fn_with_over_name, "(_swig_director int", NULL);

                p = parms;
                for (int i = 0; i < parm_count; ++i) {
                    p = getParm(p);
                    String *tm = goWrapperType(p, Getattr(p, "type"), false);
                    Printv(f_go_wrappers, ", _ ", tm, NULL);
                    Delete(tm);
                    p = nextParm(p);
                }

                Printv(f_go_wrappers, ") (_swig_ret ", go_type_name, ")", NULL);

                if (!gccgo_flag) {
                    Printv(f_go_wrappers, " {\n", NULL);
                    Printv(f_go_wrappers, "\t_swig_p := uintptr(unsafe.Pointer(&_swig_director))\n", NULL);
                    Printv(f_go_wrappers, "\t_cgo_runtime_cgocall(", wname, ", _swig_p)\n", NULL);
                    Printv(f_go_wrappers, "\treturn\n", NULL);
                    Printv(f_go_wrappers, "}", NULL);
                }

                Printv(f_go_wrappers, "\n\n", NULL);
            }

            // Write out the Go function that calls the wrapper.

            Printv(f_go_wrappers, "func ", func_with_over_name, "(v interface{}", NULL);

            p = parms;
            for (int i = 0; i < parm_count; ++i) {
                p = getParm(p);
                Printv(f_go_wrappers, ", ", Getattr(p, "lname"), " ", NULL);
                String *tm = goType(p, Getattr(p, "type"));
                Printv(f_go_wrappers, tm, NULL);
                Delete(tm);
                p = nextParm(p);
            }

            Printv(f_go_wrappers, ") *_swig_Director", cn, " {\n", NULL);

            Printv(f_go_wrappers, "\tp := &", director_struct_name, "{*SwigClass", cn ,"_setSwigcptr(0), v}\n", NULL);

            if (gccgo_flag && !cgo_flag) {
                Printv(f_go_wrappers, "\tdefer SwigCgocallDone()\n", NULL);
                Printv(f_go_wrappers, "\tSwigCgocall()\n", NULL);
            }

            String *call = NewString("");

            Printv(call, "\tp.", class_receiver, " = ", NULL);
            if (cgo_flag) {
                Printv(call, "*(*", go_type_name, ")(", class_receiver, "_setSwigcptr(uintptr(C.", wname, "(C.int(swigDirectorAdd(p))))", NULL);
            } else {
                Printv(call, fn_with_over_name, "(swigDirectorAdd(p)", NULL);
            }

            p = parms;
            for (int i = 0; i < parm_count; ++i) {
                Printv(call, ", ", NULL);

                p = getParm(p);
                String *pt = Getattr(p, "type");
                String *ln = Getattr(p, "lname");

                String *ivar = NewStringf("_swig_i_%d", i);

                String *goin = goGetattr(p, "tmap:goin");
                if (goin == NULL) {
                    Printv(f_go_wrappers, "\t", ivar, " := ", ln, NULL);
                    if (goTypeIsInterface(p, pt)) {
                        Printv(f_go_wrappers, ".Swigcptr()", NULL);
                    }
                    Printv(f_go_wrappers, "\n", NULL);
                } else {
                    String *itm = goImType(p, pt);
                    Printv(f_go_wrappers, "\tvar ", ivar, " ", itm, "\n", NULL);
                    goin = Copy(goin);
                    Replaceall(goin, "$input", ln);
                    Replaceall(goin, "$result", ivar);
                    Printv(f_go_wrappers, goin, "\n", NULL);
                    Delete(goin);
                }

                Setattr(p, "emit:goinput", ivar);

                if (cgo_flag) {
                    bool c_struct_type;
                    String *ct = cgoTypeForGoValue(p, pt, &c_struct_type);
                    if (c_struct_type) {
                        Printv(call, "*(*C.", ct, ")(unsafe.Pointer(&", ivar, "))", NULL);
                    } else {
                        Printv(call, "C.", ct, "(", ivar, ")", NULL);
                    }
                    Delete(ct);
                } else {
                    Printv(call, ivar, NULL);
                }
                p = nextParm(p);
            }

            Printv(call, ")", NULL);
            if (cgo_flag) {
                Printv(call, ")", NULL);
            }

            Printv(f_go_wrappers, call, "\n", NULL);

            goargout(parms);

            Printv(f_go_wrappers, "\treturn p\n", NULL);
            Printv(f_go_wrappers, "}\n\n", NULL);

            SwigType *result = Copy(Getattr(parentNode(n), "classtypeobj"));
            SwigType_add_pointer(result);

            Swig_save("classDirectorConstructor", n, "wrap:name", "wrap:action", NULL);

            String *dwname = Swig_name_wrapper(name);
            Append(dwname, unique_id);
            Setattr(n, "wrap:name", dwname);

            String *action = NewString("");
            Printv(action, Swig_cresult_name(), " = new SwigDirector_", class_name, "(", NULL);
            String *pname = Swig_cparm_name(NULL, 0);
            Printv(action, pname, NULL);
            Delete(pname);
            p = parms;
            for (int i = 0; i < parm_count; ++i) {
                p = getParm(p);
                String *pname = Swig_cparm_name(NULL, i + 1);
                Printv(action, ", ", NULL);
                if (SwigType_isreference(Getattr(p, "type"))) {
                    Printv(action, "*", NULL);
                }
                Printv(action, pname, NULL);
                Delete(pname);
                p = nextParm(p);
            }
            Printv(action, ");", NULL);
            Setattr(n, "wrap:action", action);

            if (cgo_flag) {
                cgoWrapperInfo info;

                info.n = n;
                info.go_name = func_name;
                info.overname = overname;
                info.wname = wname;
                info.base = NULL;
                info.parms = first_parm;
                info.result = result;
                info.is_static = false;
                info.receiver = NULL;
                info.is_constructor = true;
                info.is_destructor = false;

                int r = cgoGccWrapper(&info);
                if (r != SWIG_OK) {
                    return r;
                }
            } else if (!gccgo_flag) {
                int r = gcFunctionWrapper(wname);
                if (r != SWIG_OK) {
                    return r;
                }
                r = gccFunctionWrapper(n, NULL, wname, first_parm, result);
                if (r != SWIG_OK) {
                    return r;
                }
            } else {
                int r = gccgoFunctionWrapper(n, NULL, wname, first_parm, result);
                if (r != SWIG_OK) {
                    return r;
                }
            }

            Swig_restore(n);

            Delete(result);
        }

        String *cxx_director_name = NewString("SwigDirector_");
        Append(cxx_director_name, class_name);

        String *decl = Swig_method_decl(NULL, Getattr(n, "decl"),
            cxx_director_name, first_parm, 0, 0);
        Printv(f_c_directors_h, "  ", decl, ";\n", NULL);
        Delete(decl);

        decl = Swig_method_decl(NULL, Getattr(n, "decl"), cxx_director_name, first_parm, 0, 0);
        Printv(f_c_directors, cxx_director_name, "::", decl, "\n", NULL);
        Delete(decl);

        Printv(f_c_directors, "    : ", Getattr(parentNode(n), "classtype"), "(", NULL);

        p = parms;
        for (int i = 0; i < parm_count; ++i) {
            p = getParm(p);
            if (i > 0) {
                Printv(f_c_directors, ", ", NULL);
            }
            String *pn = Getattr(p, "name");
            assert(pn);
            Printv(f_c_directors, pn, NULL);
            p = nextParm(p);
        }
        Printv(f_c_directors, "),\n", NULL);
        Printv(f_c_directors, "      go_val(swig_p), swig_mem(0)\n", NULL);
        Printv(f_c_directors, "{ }\n\n", NULL);

        if (Getattr(n, "sym:overloaded") && !Getattr(n, "sym:nextSibling")) {
            int r = makeDispatchFunction(n, func_name, cn, is_static, Getattr(parentNode(n), "classtypeobj"), false);
            if (r != SWIG_OK) {
                return r;
            }
        }

        Delete(cxx_director_name);
        Delete(go_name);
        Delete(cn);
        Delete(go_type_name);
        Delete(director_struct_name);
        Delete(fn_name);
        Delete(fn_with_over_name);
        Delete(func_name);
        Delete(func_with_over_name);
        Delete(wname);
        Delete(first_type);
        Delete(first_parm);

        return SWIG_OK;
    }

    int classDirectorDestructor(Node *n) {
        if (!is_public(n)) {
            return SWIG_OK;
        }

        bool is_ignored = GetFlag(n, "feature:ignore") ? true : false;

        if (!is_ignored) {
            String *fnname = NewString("DeleteDirector");
            String *c1 = exportedName(class_name);
            Append(fnname, c1);
            Delete(c1);

            String *wname = Swig_name_wrapper(fnname);
            Append(wname, unique_id);

            Setattr(n, "wrap:name", fnname);

            Swig_DestructorToFunction(n, getNSpace(), getClassType(), CPlusPlus, Extend);

            ParmList *parms = Getattr(n, "parms");
            Setattr(n, "wrap:parms", parms);

            String *result = NewString("void");
            int r = makeWrappers(n, fnname, fnname, NULL, wname, NULL, parms, result, isStatic(n));
            if (r != SWIG_OK) {
                return r;
            }

            Delete(result);
            Delete(fnname);
            Delete(wname);
        }

        // Generate the destructor for the C++ director class.  Since the
        // Go code is keeping a pointer to the C++ object, we need to call
        // back to the Go code to let it know that the C++ object is gone.

        String *go_name = NewString("Swiggo_DeleteDirector_");
        Append(go_name, class_name);

        String *cn = exportedName(class_name);

        String *director_struct_name = NewString("_swig_Director");
        Append(director_struct_name, cn);

        Printv(f_c_directors_h, "  virtual ~SwigDirector_", class_name, "()", NULL);

        String *throws = buildThrow(n);
        if (throws) {
            Printv(f_c_directors_h, " ", throws, NULL);
        }

        Printv(f_c_directors_h, ";\n", NULL);

        String *director_sig = NewString("");

        Printv(director_sig, "SwigDirector_", class_name, "::~SwigDirector_", class_name, "()", NULL);

        if (throws) {
            Printv(director_sig, " ", throws, NULL);
            Delete(throws);
        }

        Printv(director_sig, "\n", NULL);
        Printv(director_sig, "{\n", NULL);

        if (is_ignored) {
            Printv(f_c_directors, director_sig, NULL);
        } else {
            makeDirectorDestructorWrapper(go_name, director_struct_name, director_sig);
        }

        Printv(f_c_directors, "  delete swig_mem;\n", NULL);

        Printv(f_c_directors, "}\n\n", NULL);

        Delete(director_sig);
        Delete(go_name);
        Delete(cn);
        Delete(director_struct_name);

        return SWIG_OK;
  }

    void makeCgoDirectorDestructorWrapper(String *go_name, String *director_struct_name, String *director_sig) {
        String *wname = Copy(go_name);
        Append(wname, unique_id);

        Printv(f_go_wrappers, "//export ", wname, "\n", NULL);
        Printv(f_go_wrappers, "func ", wname, "(c int) {\n", NULL);
        Printv(f_go_wrappers, "\tswigDirectorLookup(c).(*", director_struct_name, ").", class_receiver, " = *", class_receiver, "_setSwigcptr(0)\n", NULL);
        Printv(f_go_wrappers, "\tswigDirectorDelete(c)\n", NULL);
        Printv(f_go_wrappers, "}\n\n", NULL);

        Printv(f_c_directors, "extern \"C\" void ", wname, "(intgo);\n", NULL);
        Printv(f_c_directors, director_sig, NULL);
        Printv(f_c_directors, "  ", wname, "(go_val);\n", NULL);
    }

};
