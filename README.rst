Yuneta c-postgres
=================

Support of Postgres for Yuneta

install
-------

sudo apt-get install libpq-dev postgresql-server-dev-all

To fix this error in ubuntu 20.04 "fatal error: postgresql/libpq-fe.h: No such file or directory" ::

    sudo apt-get install --reinstall libpq-dev


Utils
-----

Number of rows::

    select COUNT(*) from tablename;
    
Size in megas::

    \dt+
    
Delete all rows::

    delete from tablename;

List tables::

    \dt+
    
Desc of table::

    \d+ tablename;
    
See last record::

    SELECT * FROM tracks_purezadb order by rowid DESC limit 1;
    
    
License
-------

Licensed under the  `The MIT License <http://www.opensource.org/licenses/mit-license>`_.
See LICENSE.txt in the source distribution for details.
