..
.. NB:  This file is machine generated, DO NOT EDIT!
..
.. Edit vmod.vcc and run make instead
..

.. role:: ref(emphasis)

.. _vmod_disco(3):

==========
vmod_disco
==========

-----------------------------------
Varnish 4.1 Auto-Discovery Backends
-----------------------------------

:Manual section: 3

SYNOPSIS
========

import disco [from "path"] ;


DESCRIPTION
===========

This is a custom load-balancing director and origin discovery service module
for Varnish 4.1.  It utilizes DNS-SD (unicast service discovery using SRV
records) to look for available backends at regular configurable intervals and
turns these into dynamic backends. This vmod requires libadns (linked at build
time) to handle the DNS communication -- all in a background thread. When
`disco.dance()` is called in vcl_recv, the latest discovered changes are
integrated into varnish backends inside the director.

The intention of this module is interoperability with backend service
configuration and provisioning systems that use DNS-SD (such as Consul.io).
This allows Varnish to operate as a front-end for such dynamically scaled
systems.

CONTENTS
========

* VOID dance(PRIV_CALL)
* Object random
* BACKEND random.backend()
* VOID random.set_probe(PROBE)
* VOID random.use_tcp()

.. _func_dance:

VOID dance(PRIV_CALL)
---------------------

Prototype
	VOID dance(PRIV_CALL)

Description
  Integrate any newly discovered backends that the background dns discovery
  thread has found as well as removing any old backends that are no longer
  discoverable. *This must be called on each request before req.backend_hint is
  set.* Normally, if the are no changes, this does nothing and consumes
  effectively no resources.
Example
  ::

    sub vcl_recv {
      disco.dance();
      set req.backend_hint = vdir.backend();
    }

.. _obj_random:

Object random
=============


Description
  Creates a new random load-balanced director (ala `directors.random()` that
  will be populated (and then automatically updated) from a DNS-SD query for
  SRV records. The duration specifies how often the query will be resent by the
  background dns thread in order to refresh the service list.
Example
  ::

    sub vcl_init {
      new vdir = disco.random("myservice.service.consul", 20s);
    }

.. _func_random.use_tcp:

VOID random.use_tcp()
---------------------

Prototype
	VOID random.use_tcp()

Description
  Use TCP rather than UDP (the default) where performing DNS-SD for this director's
  service discovery. This may be necessary for large queries due to the inherent 512
  byte DNS udp limit (and the fact that libadns does not currently support EDNS0).
Example
  ::

    sub vcl_init {
      new vdir = disco.random("myservice.service.consul", 20s);
      vdir.use_tcp();
    }

.. _func_random.set_probe:

VOID random.set_probe(PROBE)
----------------------------

Prototype
	VOID random.set_probe(PROBE)

Description
  Set the health probe to use for *all* discovered backends for this director.
  Default behavior is to not probe discovered backends.
Example
  ::

    probe myprobe {
      .url = "/foo";
      .expected_response = 201;
    }

    sub vcl_init {
      new vdir = disco.random("myservice.service.consul", 20s);
      vdir.set_probe(myprobe);
    }

.. _func_random.backend:

BACKEND random.backend()
------------------------

Prototype
	BACKEND random.backend()

Description
  Selects a random selector in exactly the same way that
  vmod_directors' `backend()` methods do.

