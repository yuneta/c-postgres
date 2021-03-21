/****************************************************************************
 *              YUNETA_POSTGRES_VERSION.H
 *              Copyright (c) 2013,2018 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yuneta_version.h>

#ifdef __cplusplus
extern "C"{
#endif

/*********************************************************************
 *      Version
 *********************************************************************/
#define __yuneta_postgres_version__  __yuneta_version__ "-postgres"
#define __yuneta_postgres_date__     __DATE__ " " __TIME__

#ifdef __cplusplus
}
#endif
