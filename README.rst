Yuneta c-postgres
=================

Support of Postgres for Yuneta

install
-------

sudo apt-get install postgresql-server-dev-all libpq-dev

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

    SELECT id,rowid,__created_at__ FROM tracks_purezadb order by rowid DESC limit 1;

    SELECT * FROM tracks_purezadb order by __created_at__ DESC limit 1;

Add new column::

    ALTER TABLE tracks_purezadb ADD COLUMN noise bigint;

License
-------

Licensed under the  `The MIT License <http://www.opensource.org/licenses/mit-license>`_.
See LICENSE.txt in the source distribution for details.
