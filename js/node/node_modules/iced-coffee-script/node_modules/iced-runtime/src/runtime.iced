
C = require('./const')

#===============================================================

make_defer_return = (obj, defer_args, id, trace_template, multi) ->

  trace = {}
  for k,v of trace_template
    trace[k] = v
  trace[C.lineno] = defer_args?[C.lineno]
  
  ret = (inner_args...) ->
    defer_args?.assign_fn?.apply(null, inner_args)
    if obj
      o = obj
      obj = null unless multi
      o._fulfill id, trace
    else
      warn "overused deferral at #{_trace_to_string trace}"

  ret[C.trace] = trace

  ret

#===============================================================

#### Tick Counter
#  count off every mod processor ticks
# 
__c = 0

tick_counter = (mod) ->
  __c++
  if (__c % mod) == 0
    __c = 0
    true
  else
    false

#===============================================================

#### Trace management and debugging
#
__active_trace = null

_trace_to_string = (tr) ->
  fn = tr[C.funcname] || "<anonymous>"
  "#{fn} (#{tr[C.filename]}:#{tr[C.lineno] + 1})"

warn = (m) ->
  console?.error "ICED warning: #{m}"

#===============================================================

####
# 
# trampoline --- make a call to the next continuation...
#   we can either do this directly, or every 500 ticks, from the
#   main loop (so we don't overwhelm ourselves for stack space)..
# 
exports.trampoline = trampoline = (fn) ->
  if not tick_counter 500
    fn()
  else if process?
    process.nextTick fn
  else
    setTimeout fn
    
#===============================================================

#### Deferrals
#
#   A collection of Deferrals; this is a better version than the one
#   that's inline; it allows for iced tracing
#

exports.Deferrals = class Deferrals

  #----------

  constructor: (k, @trace) ->
    @continuation = k
    @count = 1
    @ret = null

  #----------

  _call : (trace) ->
    if @continuation
      __active_trace = trace
      c = @continuation
      @continuation = null
      c @ret
    else
      warn "Entered dead await at #{_trace_to_string trace}"

  #----------

  _fulfill : (id, trace) ->
    if --@count > 0
      # noop
    else
      trampoline ( () => @_call trace )

  #----------
    
  defer : (args) ->
    @count++
    self = this
    return make_defer_return self, args, null, @trace

#===============================================================
    
#### findDeferral
#
# Search an argument vector for a deferral-generated callback

exports.findDeferral = findDeferral = (args) ->
  for a in args
    return a if a?[C.trace]
  null

#===============================================================
    
#### Rendezvous
#
# More flexible runtime behavior, can wait for the first deferral
# to fire, rather than just the last.

exports.Rendezvous = class Rendezvous
  constructor: ->
    @completed = []
    @waiters = []
    @defer_id = 0

  # RvId -- A helper class the allows deferalls to take on an ID
  # when used with Rendezvous
  class RvId
    constructor: (@rv,@id,@multi)->
    defer: (defer_args) ->
      @rv._defer_with_id @id, defer_args, @multi

  # Public interface
  # 
  # The public interface has 3 methods --- wait, defer and id
  # 
  wait: (cb) ->
    if @completed.length
      x = @completed.shift()
      cb(x)
    else
      @waiters.push cb

  defer: (defer_args) ->
    id = @defer_id++
    @_defer_with_id id, defer_args

  # id -- assign an ID to a deferral, and also toggle the multi
  # bit on the deferral.  By default, this bit is off.
  id: (i, multi) ->
    multi = !!multi
    new RvId(this, i, multi)

  # Private Interface

  _fulfill: (id, trace) ->
    if @waiters.length
      cb = @waiters.shift()
      cb id
    else
      @completed.push id

  _defer_with_id: (id, defer_args, multi) ->
    @count++
    make_defer_return this, defer_args, id, {}, multi

#==========================================================================

#### stackWalk
#
# Follow an iced-generated stack walk from the active trace
# up as far as we can. Output a vector of stack frames.
#
exports.stackWalk = stackWalk = (cb) ->
  ret = []
  tr = if cb then cb[C.trace] else __active_trace
  while tr
    line = "   at #{_trace_to_string tr}"
    ret.push line
    tr = tr?[C.parent]?[C.trace]
  ret

#==========================================================================

#### exceptionHandler
#
# An exception handler that triggers the above iced stack walk
# 

exports.exceptionHandler = exceptionHandler = (err, logger) ->
  logger = console.error unless logger
  logger err.stack
  stack = stackWalk()
  if stack.length
    logger "Iced 'stack' trace (w/ real line numbers):"
    logger stack.join "\n"

#==========================================================================

#### catchExceptions
# 
# Catch all uncaught exceptions with the iced exception handler.
# As mentioned here:
#
#    http://debuggable.com/posts/node-js-dealing-with-uncaught-exceptions:4c933d54-1428-443c-928d-4e1ecbdd56cb 
# 
# It's good idea to kill the service at this point, since state
# is probably horked. See his examples for more explanations.
# 
exports.catchExceptions = (logger) ->
  process?.on 'uncaughtException', (err) ->
    exceptionHandler err, logger
    process.exit 1

#==========================================================================

