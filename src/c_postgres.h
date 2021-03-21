/****************************************************************************
 *          C_POSTGRES.H
 *          Postgres GClass.
 *
 *          Postgress uv-mixin for Yuneta
 *
 *          Copyright (c) 2021 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yuneta.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/
#define GCLASS_POSTGRES_NAME "Postgres"
#define GCLASS_POSTGRES gclass_postgres()

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC GCLASS *gclass_postgres(void);

#ifdef __cplusplus
}
#endif
