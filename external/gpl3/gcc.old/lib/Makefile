#	$NetBSD: Makefile,v 1.5 2017/07/23 01:11:47 mrg Exp $

.include <bsd.own.mk>

# crtstuff is built out of elsewhere, or not at all
SUBDIR+= libgcc .WAIT
SUBDIR+= libiberty libobjc libgomp
SUBDIR+= libbacktrace
SUBDIR+= liblto_plugin

.if (${MKLIBSTDCXX} != "no") && (${MKCXX} != "no")
SUBDIR+= libsupc++ libstdc++-v3
.endif

SUBDIR+= .WAIT libasan libubsan

.include <bsd.subdir.mk>
