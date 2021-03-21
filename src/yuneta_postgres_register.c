/****************************************************************************
 *              YUNETA_POSTGRES_REGISTER.C
 *              Yuneta
 *
 *              Copyright (c) 2018 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
#include "yuneta_postgres.h"
#include "yuneta_postgres_register.h"

/***************************************************************************
 *  Data
 ***************************************************************************/

/***************************************************************************
 *  Register internal yuno gclasses and services
 ***************************************************************************/
PUBLIC int yuneta_register_c_postgres(void)
{
    static BOOL initialized = FALSE;
    if(initialized) {
        return -1;
    }

    /*
     *  Mixin uv-gobj
     */
    gobj_register_gclass(GCLASS_POSTGRES);
    initialized = TRUE;

    return 0;
}

