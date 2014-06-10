
struct LogfileInfo {
  TRI_voc_fid_t     _fid;
  std::atomic<int>  _counter = 0;
  std::atomic<bool> _wantCompaction = false;
};

class MasterPointerCollector {

  public:

    MasterPointerCollector (TRI_voc_fid_t fid) 
      : _fid(fid) {

      while (true) {
        {
          MutexLocker locker(_lock);
          if (_fidMist[fid]->_refCount == 0) {
            _fidMist[fid]->_wantCompaction = true;
            break;
          }
        }
        usleep(1000);
      }

      // refCount  == 0 && wantCompaction == true


      // compaction is done
      MutexLocker locker(_lock);
      _fidMist[fid]->_wantDeletion = true;
    }
    
    ~MasterPointerCollector () {
    }
    
};

class MasterPointerUser {

  public:

    MasterPointerUser (TRI_doc_mptr_t* mptr) {

      TRI_voc_fid_t fid = mptr->_fid;

      while (true) {
        {
          MutexLocker locker(_lock);
          if (_fidMist[fid]->_wantDeletion || 
              ! exists(_fidMist[fid])) {
            fid = mptr->_fid;
            continue;
          }

          if (! _fidMist[fid]->_wantCompaction) {
            ++_fidMist[fid]->_refCount;
            break;
          }
        }
        usleep(1000);
      }

      // refCount > 0 && wantCompaction == false

      // use mptr->_data etc.
    }

    typedef struct Mist {
      bool _wantCompaction;
      bool _wantDeletion;
      int  _refCount;
    };

    basics::Mutex _lock;

    std::map<TRI_voc_fid_t, Mist> _fidMist;
   
};
