CFLAGS = /W3 /D_CRT_SECURE_NO_DEPRECATE

build : so_stdio.dll

so_stdio.dll : so_stdio.obj
	link /nologo /dll /out:so_stdio.dll /implib:so_stdio.lib $**

so_stdio.obj : lib_generator.c stdio_internal.h \
	so_stdio.h
	$(CPP) $(CFLAGS) /c lib_generator.c /Foso_stdio.obj

clean :
	del *.obj so_stdio.dll so_stdio.exp so_stdio.lib