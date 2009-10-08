#
# $Id$
#
# This file contains a list of all subdirectories in STIR
# that will be compiled when using the Makefile

LIBDIRS += buildblock recon_buildblock display IO \
	data_buildblock \
	numerics_buildblock \
	eval_buildblock Shape_buildblock \
	listmode_buildblock \
	modelling_buildblock \
	iterative/OSMAPOSL \
	analytic/FBP2D \
	analytic/FBP3DRP

EXEDIRS += utilities recon_test \
	listmode_utilities \
	modelling_utilities \
	utilities/ecat \
	iterative/OSMAPOSL \
	iterative/POSMAPOSL \
	analytic/FBP2D \
	analytic/FBP3DRP \
	SimSET

TESTDIRS += test recon_test test/numerics test/modelling
