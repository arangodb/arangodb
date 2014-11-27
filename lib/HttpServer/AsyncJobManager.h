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
            _response(nullptr) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~AsyncCallbackContext () {
          if (_response != nullptr) {
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
          _response(nullptr),
          _stamp(0.0),
          _status(JOB_UNDEFINED),
          _ctx(nullptr) {
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

        typedef std::unordered_map<AsyncJobResult::IdType, AsyncJobResult> JobList;

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
          // remove all results that haven't been fetched
          deleteJobResults();
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

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_put_api_async_return
/// @brief Returns the result of an async job
///
/// @RESTHEADER{PUT /_api/job/job-id, Return result of an async job}
/// 
/// @RESTURLPARAMS
/// 
/// @RESTURLPARAM{job-id,string,required}
/// The async job id.
///
/// @RESTDESCRIPTION
/// Returns the result of an async job identified by job-id. If the async job 
/// result is present on the server, the result will be removed from the list of
/// result. That means this method can be called for each job-id once.
/// The method will return the original job result's headers and body, plus the 
/// additional HTTP header x-arango-async-job-id. If this header is present, then 
/// the job was found and the response contains the original job's result. If 
/// the header is not present, the job was not found and the response contains 
/// status information from the job manager.
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{204}
/// is returned if the job requested via job-id is still in the queue of pending 
/// (or not yet finished) jobs. In this case, no x-arango-async-id HTTP header 
/// will be returned.
///
/// @RESTRETURNCODE{400}
/// is returned if no job-id was specified in the request. In this case, 
/// no x-arango-async-id HTTP header will be returned.
///
/// @RESTRETURNCODE{404}
/// is returned if the job was not found or already deleted or fetched from 
/// the job result list. In this case, no x-arango-async-id HTTP header will 
/// be returned.
///
/// @EXAMPLES
/// Not providing a job-id:
///
/// ```js
/// unix> curl -X PUT --dump - http://localhost:8529/_api/job/
///
/// HTTP/1.1 400 Bad Request
/// content-type: application/json; charset=utf-8
///
/// {"error":true,"errorMessage":"bad parameter","code":400,"errorNum":400}
/// ```
///
/// Providing a job-id for a non-existing job:
///
/// ```js
/// unix> curl -X PUT --dump - http://localhost:8529/_api/job/foobar
///
/// HTTP/1.1 404 Not Found
/// content-type: application/json; charset=utf-8
///
/// {"error":true,"errorMessage":"not found","code":404,"errorNum":404}
/// ```
///
/// Fetching the result of an HTTP GET job:
///
/// ```js
/// unix> curl --header 'x-arango-async: store' --dump - http://localhost:8529/_api/version
///
/// HTTP/1.1 202 Accepted
/// content-type: text/plain; charset=utf-8
/// x-arango-async-id: 265413601
///
/// unix> curl -X PUT --dump - http://localhost:8529/_api/job/265413601
///
/// HTTP/1.1 200 OK
/// content-type: application/json; charset=utf-8
/// x-arango-async-id: 265413601
///
/// {"server":"arango","version":"2.1.0"}
/// ```
///
/// Fetching the result of an HTTP POST job that failed:
///
/// ```js
/// unix> curl -X POST --header 'x-arango-async: store' --data-binary @- --dump - http://localhost:8529/_api/collection
/// {"name":" this name is invalid "}
///
/// HTTP/1.1 202 Accepted
/// content-type: text/plain; charset=utf-8
/// x-arango-async-id: 265479137
///
/// unix> curl -X PUT --dump - http://localhost:8529/_api/job/265479137
///
/// HTTP/1.1 400 Bad Request
/// content-type: application/json; charset=utf-8
/// x-arango-async-id: 265479137
///
/// {"error":true,"code":400,"errorNum":1208,"errorMessage":"cannot create collection: illegal name"}
/// ```
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

        HttpResponse* getJobResult (AsyncJobResult::IdType jobId,
                                    AsyncJobResult::Status& status,
                                    bool removeFromList) {
          WRITE_LOCKER(_lock);

          auto it = _jobs.find(jobId);

          if (it == _jobs.end()) {
            status = AsyncJobResult::JOB_UNDEFINED;
            return nullptr;
          }

          HttpResponse* response = (*it).second._response;
          status = (*it).second._status;

          if (status == AsyncJobResult::JOB_PENDING) {
            return nullptr;
          }

          if (! removeFromList) {
            return nullptr;
          }

          // remove the job from the list
          _jobs.erase(it);
          return response;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes the result of an async job, without returning it
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_put_api_async_cancel
/// @brief Cancels the result an async job
///
/// @RESTHEADER{PUT /_api/job/job-id/cancel, Cancel async job}
/// 
/// @RESTURLPARAMS
/// 
/// @RESTURLPARAM{job-id,string,required}
/// The async job id.
///
/// @RESTDESCRIPTION
/// Cancels the currently running job identified by job-id. Note that it still 
/// might take some time to actually cancel the running async job.
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// cancel has been initiated.
///
/// @RESTRETURNCODE{400}
/// is returned if no job-id was specified in the request. In this case, 
/// no x-arango-async-id HTTP header will be returned.
///
/// @RESTRETURNCODE{404}
/// is returned if the job was not found or already deleted or fetched from 
/// the job result list. In this case, no x-arango-async-id HTTP header will 
/// be returned.
///
/// @EXAMPLES
///
/// ```js
/// unix> curl -X POST --header 'x-arango-async: store' --data-binary @- --dump - http://localhost:8529/_api/cursor
/// {"query": "FOR i IN 1..10 FOR j IN 1..10 LET x = sleep(1.0) FILTER i == 5 && j == 5 RETURN 42"}
/// 
/// HTTP/1.1 202 Accepted
/// content-type: text/plain; charset=utf-8
/// x-arango-async-id: 268952545
/// 
/// unix> curl --dump - http://localhost:8529/_api/job/pending
/// 
/// HTTP/1.1 200 OK
/// content-type: application/json; charset=utf-8
/// 
/// ["268952545"]
/// 
/// unix> curl -X PUT --dump - http://localhost:8529/_api/job/268952545/cancel
/// 
/// HTTP/1.1 200 OK
/// content-type: application/json; charset=utf-8
/// 
/// {"result":true}
/// 
/// unix> curl --dump - http://localhost:8529/_api/job/pending
/// 
/// HTTP/1.1 200 OK
/// content-type: application/json; charset=utf-8
/// 
/// ["268952545"]
/// ```
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_get_api_async_delete
/// @brief Deletes the result of an async job
///
/// @RESTHEADER{DELETE /_api/job/type, Deletes async job}
/// 
/// @RESTURLPARAMS
/// 
/// @RESTURLPARAM{type,string,required}
/// The type of jobs to delete. type can be:
/// * *all*: Deletes all jobs results. Currently executing or queued async 
///   jobs will not be stopped by this call.
/// * *expired*: Deletes expired results. To determine the expiration status of a 
///   result, pass the stamp URL parameter. stamp needs to be a UNIX timestamp, 
///   and all async job results created at a lower timestamp will be deleted.
/// * *an actual job-id*: In this case, the call will remove the result of the 
///   specified async job. If the job is currently executing or queued, it will not be aborted.
/// 
/// @RESTQUERYPARAMS
///
/// @RESTPARAM{stamp, number, optional}
///
/// A UNIX timestamp specifying the expiration threshold when type is expired.
///
/// @RESTDESCRIPTION
/// Deletes either all job results, expired job results, or the result of a specific job. 
/// Clients can use this method to perform an eventual garbage collection of job results.
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// is returned if the deletion operation was carried out successfully. 
/// This code will also be returned if no results were deleted.
///
/// @RESTRETURNCODE{400}
/// is returned if type is not specified or has an invalid value.
///
/// @RESTRETURNCODE{404}
/// is returned if type is a job-id but no async job with the specified id was found.
///
/// @EXAMPLES
///
/// Deleting all jobs:
/// 
/// ```js
/// unix> curl --header 'x-arango-async: store' --dump - http://localhost:8529/_api/version
/// 
/// HTTP/1.1 202 Accepted
/// content-type: text/plain; charset=utf-8
/// x-arango-async-id: 270132193
/// 
/// unix> curl -X DELETE --dump - http://localhost:8529/_api/job/all
/// 
/// HTTP/1.1 200 OK
/// content-type: application/json; charset=utf-8
/// 
/// { 
///   "result" : true 
/// }
/// ```
/// 
/// Deleting expired jobs:
/// 
/// ```js
/// unix> curl --header 'x-arango-async: store' --dump - http://localhost:8529/_api/version
/// 
/// HTTP/1.1 202 Accepted
/// content-type: text/plain; charset=utf-8
/// x-arango-async-id: 270197729
/// 
/// unix> curl -X DELETE --dump - http://localhost:8529/_api/job/expired?stamp=1401376184
/// 
/// HTTP/1.1 200 OK
/// content-type: application/json; charset=utf-8
/// 
/// { 
///   "result" : true 
/// }
/// ```
/// 
/// Deleting the result of a specific job:
/// 
/// ```js
/// unix> curl --header 'x-arango-async: store' --dump - http://localhost:8529/_api/version
/// 
/// HTTP/1.1 202 Accepted
/// content-type: text/plain; charset=utf-8
/// x-arango-async-id: 270263265
/// 
/// unix> curl -X DELETE --dump - http://localhost:8529/_api/job/270263265
/// 
/// HTTP/1.1 200 OK
/// content-type: application/json; charset=utf-8
/// 
/// { 
///   "result" : true 
/// }
/// ```
/// 
/// Deleting the result of a non-existing job:
/// 
/// ```js
/// unix> curl -X DELETE --dump - http://localhost:8529/_api/job/foobar
/// 
/// HTTP/1.1 404 Not Found
/// content-type: application/json; charset=utf-8
/// 
/// { 
///   "error" : true, 
///   "errorMessage" : "not found", 
///   "code" : 404, 
///   "errorNum" : 404 
/// }
/// ```
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

        bool deleteJobResult (AsyncJobResult::IdType jobId) {
          WRITE_LOCKER(_lock);

          auto it = _jobs.find(jobId);

          if (it == _jobs.end()) {
            return false;
          }

          HttpResponse* response = (*it).second._response;

          if (response != nullptr) {
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

          auto it = _jobs.begin();

          while (it != _jobs.end()) {
            HttpResponse* response = (*it).second._response;

            if (response != nullptr) {
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

          auto it = _jobs.begin();

          while (it != _jobs.end()) {
            AsyncJobResult ajr = (*it).second;

            if (ajr._stamp < stamp) {
              HttpResponse* response = ajr._response;

              if (response != nullptr) {
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

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_get_api_async_return
/// @brief Returns the status of an async job
///
/// @RESTHEADER{GET /_api/job/job-id, Returns async job}
/// 
/// @RESTURLPARAMS
/// 
/// @RESTURLPARAM{job-id,string,required}
/// The async job id.
/// 
/// @RESTDESCRIPTION
/// Returns the processing status of the specified job. The processing status can be 
/// determined by peeking into the HTTP response code of the response.
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// is returned if the job requested via job-id has been executed 
/// and its result is ready to fetch.
///
/// @RESTRETURNCODE{204}
/// is returned if the job requested via job-id is still in the queue of pending 
/// (or not yet finished) jobs.
///
/// @RESTRETURNCODE{404}
/// is returned if the job was not found or already deleted or fetched from the job result list.
///
/// @EXAMPLES
///
/// Querying the status of a done job:
/// 
/// ```js
/// unix> curl --header 'x-arango-async: store' --dump - http://localhost:8529/_api/version
/// 
/// HTTP/1.1 202 Accepted
/// content-type: text/plain; charset=utf-8
/// x-arango-async-id: 270328801
/// 
/// unix> curl --dump - http://localhost:8529/_api/job/270328801
/// 
/// HTTP/1.1 200 OK
/// content-type: text/plain; charset=utf-8
/// 
/// Querying the status of a pending job:
/// 
/// unix> curl --header 'x-arango-async: store' --dump - http://localhost:8529/_admin/sleep?duration=3
/// 
/// HTTP/1.1 202 Accepted
/// content-type: text/plain; charset=utf-8
/// x-arango-async-id: 270394337
/// 
/// unix> curl --dump - http://localhost:8529/_api/job/270394337
/// 
/// HTTP/1.1 204 No Content
/// content-type: text/plain; charset=utf-8
/// ```
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_get_api_async_return_list
/// @brief Returns the list of job result id with a specific status
///
/// @RESTHEADER{GET /_api/job/type, Returns list of async job}
/// 
/// @RESTURLPARAMS
/// 
/// @RESTURLPARAM{type,string,required}
/// The type of jobs to return. The type can be either done or pending. Setting 
/// the type to done will make the method return the ids of already completed async 
/// jobs for which results can be fetched. Setting the type to pending will return 
/// the ids of not yet finished async jobs.
/// 
/// @RESTQUERYPARAMS
///
/// @RESTPARAM{count, number, optional}
///
/// The maximum number of ids to return per call. If not specified, a 
/// server-defined maximum value will be used.
///
/// @RESTDESCRIPTION
/// Returns the list of ids of async jobs with a specific status (either done or pending). 
/// The list can be used by the client to get an overview of the job system status and 
/// to retrieve completed job results later.
///
/// @RESTRETURNCODES
/// 
/// @RESTRETURNCODE{200}
/// is returned if the list can be compiled successfully. Note: the list might be empty.
///
/// @RESTRETURNCODE{400}
/// is returned if type is not specified or has an invalid value.
///
/// @EXAMPLES
///
/// Fetching the list of done jobs:
/// 
/// ```js
/// unix> curl --header 'x-arango-async: store' --dump - http://localhost:8529/_api/version
/// 
/// HTTP/1.1 202 Accepted
/// content-type: text/plain; charset=utf-8
/// x-arango-async-id: 270459873
/// 
/// unix> curl --dump - http://localhost:8529/_api/job/done
/// 
/// HTTP/1.1 200 OK
/// content-type: application/json; charset=utf-8
/// 
/// [ 
///   "270459873" 
/// ]
/// ```
/// 
/// Fetching the list of pending jobs:
/// 
/// ```js
/// unix> curl --header 'x-arango-async: store' --dump - http://localhost:8529/_api/version
/// 
/// HTTP/1.1 202 Accepted
/// content-type: text/plain; charset=utf-8
/// x-arango-async-id: 270525409
/// 
/// unix> curl --dump - http://localhost:8529/_api/job/pending
/// 
/// HTTP/1.1 200 OK
/// content-type: application/json; charset=utf-8
/// 
/// [ ]
/// ```
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

        const std::vector<AsyncJobResult::IdType> byStatus (AsyncJobResult::Status status,
                                                            size_t maxCount) {
          std::vector<AsyncJobResult::IdType> jobs;
          size_t n = 0;

          {
            READ_LOCKER(_lock);
            auto it = _jobs.begin();

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

          TRI_ASSERT(job != nullptr);

          *jobId = (AsyncJobResult::IdType) generate();
          job->assignId((uint64_t) *jobId);

          AsyncCallbackContext* ctx = nullptr;

          bool found;
          char const* hdr = job->getHandler()->getRequest()->header("x-arango-coordinator", found);

          if (found) {
            LOG_DEBUG("Found header X-Arango-Coordinator in async request");
            ctx = new AsyncCallbackContext(std::string(hdr));
          }

          AsyncJobResult ajr(*jobId,
                             nullptr,
                             TRI_microtime(),
                             AsyncJobResult::JOB_PENDING,
                             ctx);

          WRITE_LOCKER(_lock);

          _jobs.emplace(*jobId, ajr);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief finishes the execution of an async job
////////////////////////////////////////////////////////////////////////////////

        template<typename S, typename HF>
        void finishAsyncJob (GeneralServerJob<S, typename HF::GeneralHandler>* job) {
          TRI_ASSERT(job != nullptr);

          typename HF::GeneralHandler* handler = job->getHandler();
          TRI_ASSERT(handler != nullptr);

          AsyncJobResult::IdType jobId = job->id();

          if (jobId == 0) {
            return;
          }

          double const now = TRI_microtime();
          AsyncCallbackContext* ctx = nullptr;
          HttpResponse* response    = nullptr;

          {
            WRITE_LOCKER(_lock);
            auto it = _jobs.find(jobId);

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

              if (ctx != nullptr) {
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

          if (nullptr != ctx && nullptr != callback) {
            callback(ctx->getCoordinatorHeader(), response);
            delete ctx;
            if (response != nullptr) {
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
