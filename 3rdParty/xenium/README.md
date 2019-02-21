# xenium

[![Build Status](https://travis-ci.org/mpoeter/xenium.svg?branch=master)](https://travis-ci.org/mpoeter/xenium)
[![MIT Licensed](https://img.shields.io/badge/license-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Documentation](https://img.shields.io/badge/docs-doxygen-orange.svg)](https://mpoeter.github.io/xenium)

xenium is a collection of concurrent data structures and memory reclamation algorithms.
The data structures are parameterized so that they can be used with various reclamation
schemes (similar to how the STL allows customization of allocators). The [documentation](https://mpoeter.github.io/xenium) provides more details.

This project is based on the previous work in https://github.com/mpoeter/emr

### Data Structures
At the moment the number of provided data structures is rather small since the focus so far
was on the reclamation schemes. However, the plan is to add several more data structures in
the near future.

* `michael_scott_queue` - an unbounded lock-free multi-producer/multi-consumer queue proposed by
Michael and Scott \[[MS96](#ref-michael-1996)\].
* `ramalhete_queue` - a fast unbounded lock-free multi-producer/multi-consumer queue proposed by
Ramalhete \[[Ram16](#ref-ramalhete-2016)\].
* `harris_michael_list_based_set` - a lock-free container that contains a sorted set of unique objects.
This data structure is based on the solution proposed by Michael \[[Mic02](#ref-michael-2002)\] which builds
upon the original proposal by Harris \[[Har01](#ref-harris-2001)\].
* `harris_michael_hash_map` - a lock-free hash-map based on the solution proposed by Michael
\[[Mic02](#ref-michael-2002)\] which builds upon the original proposal by Harris \[[Har01](#ref-harris-2001)\].
* `chase_work_stealing_deque` - a work stealing deque based on the proposal by
Chase and Lev \[[CL05](#ref-chase-2005)\].
* `vyukov_hash_map` - a concurrent hash-map that uses fine grained locking for update operations.
This implementation is heavily inspired by the version proposed by Vyukov \[[Vyu08](#ref-vyukov-2008)\].

### Reclamation Schemes

The implementation of the reclamation schemes is based on an adapted version of the interface
proposed by Robison \[[Rob13](#ref-robison-2013)\].

The following reclamation schemes are implemented:
* `lock_free_ref_count` \[[Val95](#ref-valois-1995), [MS95](#ref-michael-1995)\]
* `hazard_pointer` \[[Mic04](#ref-michael-2004)\]
* `hazard_eras` \[[RC17](#ref-ramalhete-2017)\]
* `quiescent_state_based`
* `epoch_based` \[[Fra04](#ref-fraser-2004)\]
* `new_epoch_based` \[[HMBW07](#ref-hart-2007)\]
* `debra` \[[Bro15](#ref-brown-2015)\]
* `stamp_it` \[[PT18a](#ref-pöter-2018), [PT18b](#ref-pöter-2018-tr)\]

#### References

<table style="border:0px">
<tr>
    <td valign="top"><a name="ref-brown-2015"></a>[Bro15]</td>
    <td>Trevor Alexander Brown.
    <a href=http://www.cs.utoronto.ca/~tabrown/debra/paper.podc15.pdf>
    Reclaiming memory for lock-free data structures: There has to be a better way</a>.
    In <i>Proceedings of the 2015 ACM Symposium on Principles of Distributed Computing (PODC)</i>,
    pages 261–270. ACM, 2015.</td>
</tr>
<tr>
    <td valign="top"><a name="ref-chase-2005"></a>[CL05]</td>
    <td>David Chase and Yossi Lev.
    <a href=https://www.dre.vanderbilt.edu/~schmidt/PDF/work-stealing-dequeue.pdf>
    Dynamic circular work-stealing deque</a>.
    In <i>Proceedings of the 17th Annual ACM Symposium on Parallelism in Algorithms and Architectures (SPAA)</i>,
    pages 21–28. ACM, 2005.</td>
</tr>
<tr>
    <td valign="top"><a name="ref-fraser-2004"></a>[Fra04]</td>
    <td>Keir Fraser.
    <a href=https://www.cl.cam.ac.uk/techreports/UCAM-CL-TR-579.pdf>
    <i>Practical lock-freedom</i></a>.
    PhD thesis, University of Cambridge Computer Laboratory, 2004.</td>
</tr>
<tr>
    <td valign="top"><a name="ref-harris-2001"></a>[Har01]</td>
    <td>Timothy L. Harris.
    <a href=https://www.cl.cam.ac.uk/research/srg/netos/papers/2001-caslists.pdf>
    A pragmatic implementation of non-blocking linked-lists</a>.
    In <i>Proceedings of the 15th International Conference on Distributed Computing (DISC)</i>,
    pages 300–314. Springer-Verlag, 2001.</td>
</tr>
<tr>
    <td valign="top"><a name="ref-hart-2007"></a>[HMBW07]</td>
    <td>Thomas E. Hart, Paul E. McKenney, Angela Demke Brown, and Jonathan Walpole.
    <a href=http://csng.cs.toronto.edu/publication_files/0000/0159/jpdc07.pdf>
    Performance of memory reclamation for lockless synchronization</a>.
    Journal of Parallel and Distributed Computing, 67(12):1270–1285, 2007.</td>
</tr>
<tr>
    <td valign="top"><a name="ref-michael-2002"></a>[Mic02]</td>
    <td>Maged M. Michael.
    <a href=http://www.liblfds.org/downloads/white%20papers/%5BHash%5D%20-%20%5BMichael%5D%20-%20High%20Performance%20Dynamic%20Lock-Free%20Hash%20Tables%20and%20List-Based%20Sets.pdf>
    High performance dynamic lock-free hash tables and list-based sets</a>.
    In <i>Proceedings of the 14th Annual ACM Symposium on Parallel Algorithms and Architectures
    (SPAA)</i>, pages 73–82. ACM, 2002.</td>
</tr>
<tr>
    <td valign="top"><a name="ref-michael-2004"></a>[Mic04]</td>
    <td>Maged M. Michael.
    <a href=http://www.cs.otago.ac.nz/cosc440/readings/hazard-pointers.pdf>
    Hazard pointers: Safe memory reclamation for lock-free objects</a>.
    IEEE Transactions on Parallel and Distributed Systems, 15(6):491–504, 2004.</td>
</tr>
<tr>
    <td valign="top"><a name="ref-michael-1995"></a>[MS95]</td>
    <td>Maged M. Michael and Michael L. Scott.
    <a href=https://pdfs.semanticscholar.org/cec0/ad7b0fc2d4d6ba45c6212d36217df1ff2bf2.pdf>
    Correction of a memory management method for lock-free data structures</a>.
    Technical report, University of Rochester, 1995.</td>
</tr>
<tr>
    <td valign="top"><a name="ref-michael-1996"></a>[MS96]</td>
    <td>Maged M. Michael and Michael L. Scott.
    <a href=http://www.cs.rochester.edu/~scott/papers/1996_PODC_queues.pdf>
    Simple, fast, and practical non-blocking and blocking concurrent queue algorithms</a>.
    In <i>Proceedings of the 15th Annual ACM Symposium on Principles of Distributed Computing (PODC)</i>,
    pages 267–275. ACM, 1996.</td>
</tr>
<tr>
    <td valign="top"><a name="ref-pöter-2018"></a>[PT18a]</td>
    <td>Manuel Pöter and Jesper Larsson Träff.
    Brief announcement: Stamp-it, a more thread-efficient, concurrent memory reclamation scheme in the C++ memory model.
    In <i>Proceedings of the 30th Annual ACM Symposium on Parallelism in Algorithms and Architectures (SPAA)</i>,
    pages 355–358. ACM, 2018.</td>
</tr>
<tr>
    <td valign="top"><a name="ref-pöter-2018-tr"></a>[PT18b]</td>
    <td>Manuel Pöter and Jesper Larsson Träff.
    <a href=https://arxiv.org/pdf/1805.08639.pdf>
    Stamp-it, a more thread-efficient, concurrent memory reclamation scheme in the C++ memory model</a>.
    Technical report, 2018.</td>
</tr>
<tr>
    <td valign="top"><a name="ref-ramalhete-2016"></a>[Ram16]</td>
    <td>Pedro Ramalhete.
    <a href=http://concurrencyfreaks.blogspot.com/2016/11/faaarrayqueue-mpmc-lock-free-queue-part.html>
    FAAArrayQueue - MPMC lock-free queue (part 4 of 4)</a>.
    Blog, November 2016.</td>
</tr>
<tr>
    <td valign="top"><a name="ref-ramalhete-2017"></a>[RC17]</td>
    <td>Pedro Ramalhete and Andreia Correia.
    <a href=https://github.com/pramalhe/ConcurrencyFreaks/blob/master/papers/hazarderas-2017.pdf>
    Brief announcement: Hazard eras - non-blocking memory reclamation</a>.
    In <i>Proceedings of the 29th Annual ACM Symposium on Parallelism in Algorithms and Architectures (SPAA)</i>,
    pages 367–369. ACM, 2017.</td>
</tr>
<tr>
    <td valign="top"><a name="ref-robison-2013"></a>[Rob13]</td>
    <td>Arch D. Robison.
    <a href=http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2013/n3712.pdf>
    Policy-based design for safe destruction in concurrent containers</a>.
    C++ standards committee paper, 2013.</td>
</tr>
<tr>
    <td valign="top"><a name="ref-valois-1995"></a>[Val95]</td>
    <td>John D. Valois. <i>Lock-Free Data Structures</i>.
    PhD thesis, Rensselaer Polytechnic Institute, 1995.</td>
</tr>
<tr>
    <td valign="top"><a name="ref-vyukov-2008"></a>[Vyu08]</td>
    <td>Dmitry Vyukov.
    <a href=https://groups.google.com/forum/#!topic/lock-free/qCYGGkrwbcA>
    Scalable hash map</a>. Google Groups posting, 2008.</td>
</tr>
</table>

