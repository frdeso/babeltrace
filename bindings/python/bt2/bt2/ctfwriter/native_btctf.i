/* Output argument typemap for field type output (always appends) */
%typemap(in, numinputs=0) struct bt_ctf_field_type **BTCTFOUTFT (struct bt_ctf_field_type *temp_ft = NULL) {
	$1 = &temp_ft;
}

%typemap(argout) struct bt_ctf_field_type **BTCTFOUTFT {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result, SWIG_NewPointerObj(SWIG_as_voidptr(*$1), SWIGTYPE_p_bt_ctf_field_type, 0));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}
