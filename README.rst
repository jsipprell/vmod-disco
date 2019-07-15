..
.. NB:  This file is machine generated, DO NOT EDIT!
..
.. Edit vmod.vcc and run make instead
..

.. role:: ref(emphasis)


==========
VMOD disco
==========

-----------------------------------
Varnish 6.2 Auto-Discovery Backends
-----------------------------------

:Manual section: 3

SYNOPSIS
========

.. parsed-literal::

  import disco [from "path"]
  
  VOID dance()
  
  new xround_robin = disco.round_robin(STRING, DURATION)
  
      VOID xround_robin.use_tcp()
   
      VOID xround_robin.set_probe(PROBE)
   
      BACKEND xround_robin.backend()
   
  new xrandom = disco.random(STRING, DURATION)
  
      VOID xrandom.use_tcp()
   
      VOID xrandom.set_probe(PROBE)
   
      BACKEND xrandom.backend()
   
DESCRIPTION
===========

This is a custom load-balancing director and origin discovery service module
for Varnish 6.2.  It utilizes DNS-SD (unicast service discovery using SRV
records) to look for available backends at regular configurable intervals and
turns these into dynamic backends. This vmod requires libadns (linked at build
time) to handle the DNS communication -- all in a background thread. When
`disco.dance()` is called in vcl_recv, the latest discovered changes are
integrated into varnish backends inside the director.

The intention of this module is interoperability with backend service
configuration and provisioning systems that use DNS-SD (such as Consul.io).
This allows Varnish to operate as a front-end for such dynamically scaled
systems.

.. _vmod_disco.dance:

VOID dance()
------------

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

.. _vmod_disco.round_robin:

new xround_robin = disco.round_robin(STRING, DURATION)
------------------------------------------------------

Description
  Creates a new round-robin load-balanced director (ala `directors.round_robin()` that
  will be populated (and then automatically updated) from a DNS-SD query for
  SRV records. The duration specifies how often the query will be resent by the
  background dns thread in order to refresh the service list.
Example
  ::

    sub vcl_init {
      new vdir = disco.round_robin("myservice.service.consul", 20s);
    }

.. _vmod_disco.round_robin.use_tcp:

VOID xround_robin.use_tcp()
---------------------------

Description
  Use TCP rather than UDP (the default) where performing DNS-SD for this director's
  service discovery. This may be necessary for large queries due to the inherent 512
  byte DNS udp limit (and the fact that libadns does not currently support EDNS0).
Example
  ::

    sub vcl_init {
      new vdir = disco.round_robin("myservice.service.consul", 20s);
      vdir.use_tcp();
    }

.. _vmod_disco.round_robin.set_probe:

VOID xround_robin.set_probe(PROBE)
----------------------------------

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
      new vdir = disco.round_robin("myservice.service.consul", 20s);
      vdir.set_probe(myprobe);
    }

.. _vmod_disco.round_robin.backend:

BACKEND xround_robin.backend()
------------------------------

Description
  Returns a round-robin backend selector in exactly the same way that
  vmod_directors' `backend()` methods do.

.. _vmod_disco.random:

new xrandom = disco.random(STRING, DURATION)
--------------------------------------------

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

.. _vmod_disco.random.use_tcp:

VOID xrandom.use_tcp()
----------------------

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

.. _vmod_disco.random.set_probe:

VOID xrandom.set_probe(PROBE)
-----------------------------

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

.. _vmod_disco.random.backend:

BACKEND xrandom.backend()
-------------------------

Description
  Returns a random backend selector in exactly the same way that
  vmod_directors' `backend()` methods do.
