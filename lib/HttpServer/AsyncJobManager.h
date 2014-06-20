////////////////////////////////////////////////////////////////////////////////
/// @brief job manager
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2004-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_HTTP_SERVER_ASYNC_JOB_MANAGER_H
#define ARANGODB_HTTP_SERVER_ASYNC_JOB_MANAGER_H 1

#include "Basics/Common.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "GeneralServer/GeneralServerJob.h"

namespace triagens {
  namespace rest {

// -----------------------------------------------------------------------------
// --SECTION--                                        class AsyncCallbackContext
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief AsyncCallbackContext
////////////////////////////////////////////////////////////////////////////////

    class AsyncCallbackContext {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        AsyncCallbackContext (std::string const& coordHeader)
          : _coordHeader(coordHeader),
            _response(0) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~AsyncCallbackContext () {
          if (_response != 0) {
            delete _response;
          }
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the coordinator header
////////////////////////////////////////////////////////////////////////////////

        std::string& getCoordinatorHeader () {
          return _coordHeader;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief coordinator header
////////////////////////////////////////////////////////////////////////////////

        std::string _coordHeader;

////////////////////////////////////////////////////////////////////////////////
/// @brief http response
////////////////////////////////////////////////////////////////////////////////

        HttpResponse* _response;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                              class AsyncJobResult
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief AsyncJobResult
////////////////////////////////////////////////////////////////////////////////

    struct AsyncJobResult {

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief job states
////////////////////////////////////////////////////////////////////////////////

        typedef enum {
          JOB_UNDEFINED,
          JOB_PENDING,
          JOB_DONE
        }
        Status;

////////////////////////////////////////////////////////////////////////////////
/// @brief id typedef
////////////////////////////////////////////////////////////////////////////////

        typedef uint64_t IdType;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor for an unspecified job result
////////////////////////////////////////////////////////////////////////////////

        AsyncJobResult () :
          _jobId(0),
          _response(0),
          _stamp(0.0),
          _status(JOB_UNDEFINED),
          _ctx(0) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor for a specific job result
////////////////////////////////////////////////////////////////////////////////

        AsyncJobResult (IdType jobId,
                        HttpResponse* response,
                        double stamp,
                        Status status,
                        AsyncCallbackContext* ctx) :
          _jobId(jobId),
          _response(response),
          _stamp(stamp),
          _status(status),
          _ctx(ctx) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~AsyncJobResult () {
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief job id
////////////////////////////////////////////////////////////////////////////////

        IdType _jobId;

////////////////////////////////////////////////////////////////////////////////
/// @brief the full HTTP response object of the job, can be 0
////////////////////////////////////////////////////////////////////////////////

        HttpResponse* _response;

////////////////////////////////////////////////////////////////////////////////
/// @brief job creation stamp
////////////////////////////////////////////////////////////////////////////////

        double _stamp;

////////////////////////////////////////////////////////////////////////////////
/// @brief job status
////////////////////////////////////////////////////////////////////////////////

        Status _status;

////////////////////////////////////////////////////////////////////////////////
/// @brief callback context object (normally 0, used in cluster operations)
////////////////////////////////////////////////////////////////////////////////

        AsyncCallbackContext* _ctx;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                             class AsyncJobManager
// -----------------------------------------------------------------------------

    class AsyncJobManager {

      private:
        AsyncJobManager (AsyncJobManager const&);
        AsyncJobManager& operator= (AsyncJobManager const&);

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief callback typedef
////////////////////////////////////////////////////////////////////////////////

        typedef void (*callback_fptr)(std::string&, HttpResponse*);

////////////////////////////////////////////////////////////////////////////////
/// @brief generate typedef
////////////////////////////////////////////////////////////////////////////////

        typedef uint64_t (*generate_fptr)();

////////////////////////////////////////////////////////////////////////////////
/// @brief joblist typedef
////////////////////////////////////////////////////////////////////////////////

        typedef std::map<AsyncJobResult::IdType, AsyncJobResult> JobList;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        AsyncJobManager (generate_fptr idFunc,
                         callback_fptr callback)
          : _lock(),
            _jobs(),
            generate(idFunc),
            callback(callback) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~AsyncJobManager () {
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the result of an async job
///
/// Get the result of an async job, and optionally remove it from the list of
/// done jobs if it is completed.
////////////////////////////////////////////////////////////////////////////////

        HttpResponse* getJobResult (AsyncJobResult::IdType jobId,
                                    AsyncJobResult::Status& status,
                                    bool removeFromList) {
          WRITE_LOCKER(_lock);

          JobList::iterator it = _jobs.find(jobId);

          if (it == _jobs.end()) {
            status = AsyncJobResult::JOB_UNDEFINED;
            return 0;
          }

          HttpResponse* response = (*it).second._response;
          status = (*it).second._status;

          if (status == AsyncJobResult::JOB_PENDING) {
            return 0;
          }

          if (! removeFromList) {
            return 0;
          }

          // remove the job from the list
          _jobs.erase(it);
          return response;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes the result of an async job, without returning it
////////////////////////////////////////////////////////////////////////////////

        bool deleteJobResult (AsyncJobResult::IdType jobId) {
          WRITE_LOCKER(_lock);

          JobList::iterator it = _jobs.find(jobId);

          if (it == _jobs.end()) {
            return false;
          }

          HttpResponse* response = (*it).second._response;

          if (response != 0) {
            delete response;
          }

          // remove the job from the list
          _jobs.erase(it);
          return true;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes all results
////////////////////////////////////////////////////////////////////////////////

        void deleteJobResults () {
          WRITE_LOCKER(_lock);

          JobList::iterator it = _jobs.begin();

          while (it != _jobs.end()) {
            HttpResponse* response = (*it).second._response;

            if (response != 0) {
              delete response;
            }

            ++it;
          }

          _jobs.clear();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes expired results
////////////////////////////////////////////////////////////////////////////////

        void deleteExpiredJobResults (double stamp) {
          WRITE_LOCKER(_lock);

          JobList::iterator it = _jobs.begin();

          while (it != _jobs.end()) {
            AsyncJobResult ajr = (*it).second;

            if (ajr._stamp < stamp) {
              HttpResponse* response = ajr._response;

              if (response != 0) {
                delete response;
              }

              _jobs.erase(it++);
            }
            else {
              ++it;
            }
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the list of pending jobs
////////////////////////////////////////////////////////////////////////////////

        const std::vector<AsyncJobResult::IdType> pending (size_t maxCount) {
          return byStatus(AsyncJobResult::JOB_PENDING, maxCount);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the list of done jobs
////////////////////////////////////////////////////////////////////////////////

        const std::vector<AsyncJobResult::IdType> done (size_t maxCount) {
          return byStatus(AsyncJobResult::JOB_DONE, maxCount);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the list of jobs by status
////////////////////////////////////////////////////////////////////////////////

        const std::vector<AsyncJobResult::IdType> byStatus (AsyncJobResult::Status status,
                                                            size_t maxCount) {
          vector<AsyncJobResult::IdType> jobs;
          size_t n = 0;

          {
            READ_LOCKER(_lock);
            JobList::iterator it = _jobs.begin();

            // iterate the list. the list is sorted by id
            while (it != _jobs.end()) {
              AsyncJobResult::IdType jobId = (*it).first;

              if ((*it).second._status == status) {
                jobs.push_back(jobId);

                if (++n >= maxCount) {
                  break;
                }
              }

              ++it;
            }
          }

          return jobs;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises an async job
////////////////////////////////////////////////////////////////////////////////

        template<typename S, typename HF>
        void initAsyncJob (GeneralServerJob<S, typename HF::GeneralHandler>* job,
                           uint64_t* jobId) {
          if (jobId == 0) {
            return;
          }

          TRI_ASSERT(job != 0);

          *jobId = (AsyncJobResult::IdType) generate();
          job->assignId((uint64_t) *jobId);

          AsyncCallbackContext* ctx = 0;

          bool found;
          char const* hdr = job->getHandler()->getRequest()->header("x-arango-coordinator", found);

          if (found) {
            LOG_DEBUG("Found header X-Arango-Coordinator in async request");
            ctx = new AsyncCallbackContext(std::string(hdr));
          }

          AsyncJobResult ajr(*jobId,
                             0,
                             TRI_microtime(),
                             AsyncJobResult::JOB_PENDING,
                             ctx);

          WRITE_LOCKER(_lock);

          _jobs[*jobId] = ajr;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief finishes the execution of an async job
////////////////////////////////////////////////////////////////////////////////

        template<typename S, typename HF>
        void finishAsyncJob (GeneralServerJob<S, typename HF::GeneralHandler>* job) {
          TRI_ASSERT(job != 0);

          typename HF::GeneralHandler* handler = job->getHandler();
          TRI_ASSERT(handler != 0);

          AsyncJobResult::IdType jobId = job->id();

          if (jobId == 0) {
            return;
          }

          const double now = TRI_microtime();
          AsyncCallbackContext* ctx = 0;
          HttpResponse* response    = 0;

          {
            WRITE_LOCKER(_lock);
            JobList::iterator it = _jobs.find(jobId);

            if (it == _jobs.end()) {
              // job is already deleted.
              // do nothing here. the dispatcher will throw away the handler,
              // which will also dispose the response
              return;
            }
            else {
              response = handler->stealResponse();

              (*it).second._response = response;
              (*it).second._status = AsyncJobResult::JOB_DONE;
              (*it).second._stamp = now;

              ctx = (*it).second._ctx;

              if (ctx != 0) {
                // we have found a context object, so we can immediately remove the job
                // from the list of "done" jobs
                _jobs.erase(it);
              }
            }
          }

          // If we got here, then we have stolen the pointer to the response

          // If there is a callback context, the job is no longer in the
          // list of "done" jobs, so we have to free the response and the
          // callback context:

          if (0 != ctx && 0 != callback) {
            callback(ctx->getCoordinatorHeader(), response);
            delete ctx;
            if (response != 0) {
              delete response;
            }
          }
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief lock to protect the _jobs map
////////////////////////////////////////////////////////////////////////////////

        basics::ReadWriteLock _lock;

////////////////////////////////////////////////////////////////////////////////
/// @brief list of pending/done async jobs
////////////////////////////////////////////////////////////////////////////////

        JobList _jobs;

////////////////////////////////////////////////////////////////////////////////
/// @brief function pointer for id generation
////////////////////////////////////////////////////////////////////////////////

        generate_fptr generate;

////////////////////////////////////////////////////////////////////////////////
/// @brief function pointer for callback registered at initialisation
////////////////////////////////////////////////////////////////////////////////

        callback_fptr callback;
    };
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
