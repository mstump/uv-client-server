// This is free and unencumbered software released into the public domain.

// Anyone is free to copy, modify, publish, use, compile, sell, or
// distribute this software, either in source code form or as a compiled
// binary, for any purpose, commercial or non-commercial, and by any
// means.

// In jurisdictions that recognize copyright laws, the author or authors
// of this software dedicate any and all copyright interest in the
// software to the public domain. We make this dedication for the benefit
// of the public at large and to the detriment of our heirs and
// successors. We intend this dedication to be an overt act of
// relinquishment in perpetuity of all present and future rights to this
// software under copyright law.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

// For more information, please refer to <http://unlicense.org/>

#ifndef __REQUEST_HPP_INCLUDED__
#define __REQUEST_HPP_INCLUDED__

#include <atomic>
#include <uv.h>

template<typename Work,
         typename Result>
struct request_t {

    typedef std::function<void(request_t<Work,Result>*)> callable_t;

    std::atomic<bool>       flag;
    std::mutex              mutex;
    std::condition_variable condition;
    Work                    work;
    Result                  result;
    int                     error;
    callable_t              callback;
    uv_work_t               uv_work_req;


    request_t() :
        flag(false),
        error(CQL_ERROR_NO_ERROR),
        callback(NULL)
    {}

    bool
    ready()
    {
        return flag.load(std::memory_order_consume);
    }

    /**
     * call to set the ready condition to true and notify interested parties
     * threads blocking on wait or wait_for will be woken up and resumed
     * if the caller specified a callback it will be triggered
     *
     * be sure to set the value of result prior to calling notify
     *
     * we execute the callback in a separate thread so that badly
     * behaving client code can't interfere with event/network handling
     *
     * @param loop the libuv event loop
     */
    void
    notify(
        uv_loop_t* loop)
    {
        flag.store(true, std::memory_order_release);
        condition.notify_all();

        if (callback) {
            // we execute the callback in a separate thread so that badly
            // behaving client code can't interfere with event/network handling
            uv_work_req.data = this;
            uv_queue_work(loop, &uv_work_req, &request_t<Work, Result>::callback_executor, NULL);
        }
    }

    /**
     * wait until the ready condition is met and results are ready
     */
    void
    wait()
    {
        if (!flag.load(std::memory_order_consume)) {
            std::unique_lock<std::mutex> lock(mutex);
            condition.wait(lock, std::bind(&request_t<Work, Result>::ready, this));
        }
    }

    /**
     * wait until the ready condition is met, or specified time has elapsed.
     * return value indicates false for timeout
     *
     * @param time
     *
     * @return false for timeout, true if value is ready
     */
    template< class Rep, class Period >
    bool
    wait_for(
        const std::chrono::duration<Rep, Period>& time)
    {
        if (!flag.load(std::memory_order_consume)) {
            std::unique_lock<std::mutex> lock(mutex);
            return condition.wait_for(lock, time, std::bind(&request_t<Work, Result>::ready, this));
        }
    }

private:
    /**
     * Called by the libuv worker thread, and this method calls the callback
     * this is done to isolate customer code from ours
     *
     * @param work
     */
    static void
    callback_executor(
        uv_work_t* work)
    {
        (void) work;
        if (work && work->data) {
            request_t<Work,Result>* request = (request_t<Work,Result>*) work->data;
            if (request->callback) {
                request->callback(request);
            }
        }
    }

    // don't allow copy
    request_t(request_t&) {}
    void operator=(const request_t&) {}
};

#endif
