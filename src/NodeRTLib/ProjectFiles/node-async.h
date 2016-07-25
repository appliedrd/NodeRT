// Copyright (c) Microsoft Corporation
// All rights reserved. 
//
// Licensed under the Apache License, Version 2.0 (the ""License""); you may not use this file except in compliance with the License. You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0 
//
// THIS CODE IS PROVIDED ON AN  *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT. 
//
// See the Apache Version 2.0 License for specific language governing permissions and limitations under the License.

#pragma once

#include <v8.h>
#include "nan.h"
#include <TlHelp32.h>

#include <functional>
#include <memory>
#include <vector>

#if NAUV_UVVERSION < 0x000b17
#define NODEASYNC_ASYNC_WORK_CB(func) \
    static void __cdecl func(uv_async_t *handle, int)
#define NODEASYNC_IDLE_WORK_CB(func) \
    static void __cdecl func(uv_idle_t *handle, int)
#else
#define NODEASYNC_ASYNC_WORK_CB(func) \
    static void __cdecl func(uv_async_t *handle)
#define NODEASYNC_IDLE_WORK_CB(func) \
    static void __cdecl func(uv_idle_t *handle)
#endif

namespace NodeUtils
{
  using v8::Integer;
  using v8::String;
  using v8::Function;
  using v8::Exception;
  using v8::Object;
  using v8::Local;
  using v8::Handle;
  using v8::Value;
  using Nan::New;
  using Nan::HandleScope;
  using Nan::GetCurrentContext;
  using Nan::EscapableHandleScope;
  using Nan::MakeCallback;
  using Nan::Null;
  using Nan::Persistent;
  using Nan::Undefined;
  
  typedef std::function<void (int, Handle<Value>*)> InvokeCallbackDelegate;
  
  class Async
  {
  public:
    template<typename TInput, typename TResult> 
    struct Baton 
    {
      int error_code;
      std::wstring error_message;

      // Custom data
      std::shared_ptr<TInput> data;
      std::shared_ptr<TResult> result;
      std::shared_ptr<Persistent<Value>> callback_args;
      unsigned callback_args_size;

      Baton() 
      {
        callback_args_size = 0;
      }

      void setCallbackArgs(Handle<Value>* argv, int argc)
      {
        HandleScope scope;

        callback_info.reset(new Persistent<Value>[argc], [](Persistent<Value> * ptr) {
          delete [] ptr;
        });

        callback_args_size = 0;

        for (int i=0; i<argc; i++)
        {
          //callback_info.get()[i] = argv[i];
          callback_info.get()[i].Reset(argv[i]);
        }
      }
      
      virtual ~Baton()
      {
        for (int i = 0; i < callback_args_size; i++)
        {
          callback_info.get()[i].Reset();
        }
      }

    private:
      uv_work_t request;
      std::function<void (Baton*)> doWork;
      std::function<void (Baton*)> afterWork;
      Nan::Persistent<Object> callbackData;

      friend Async;
    };

  private:
    class TokenData
    {
    public:
      static uv_async_t* NewAsyncToken()
      {
        uv_async_t* asyncHandle = new uv_async_t;
        uv_async_init(uv_default_loop(), asyncHandle, AsyncCb);
        SetHandleData(asyncHandle->data);

        return asyncHandle;
      }

      static uv_async_t* NewAsyncToken(
        Handle<Function> callback, 
        Handle<Value> receiver)
      {
        uv_async_t* asyncHandle = NewAsyncToken();
        SetHandleCallbackData(asyncHandle->data, callback, receiver);

        return asyncHandle;
      }

      static uv_idle_t* NewIdleToken()
      {
        uv_idle_t* idleHandle = new uv_idle_t;
        uv_idle_init(uv_default_loop(), idleHandle);

        SetHandleData(idleHandle->data);
        return idleHandle;
      }

      static uv_idle_t* NewIdleToken(
        Handle<Function> callback, 
        Handle<Value> receiver)
      {
        uv_idle_t* idleHandle = NewIdleToken();
        SetHandleCallbackData(idleHandle->data, callback, receiver);

        return idleHandle;
      }

      virtual ~TokenData()
      {
        callbackData.Reset();
      }

    private:
      static void SetHandleData(void*& handleData)
      {
        handleData = new TokenData();
      }

      static void SetHandleCallbackData(
        void* handleData,
        Handle<Function> callback, 
        Handle<Value> receiver)
      {
        TokenData* Token = static_cast<TokenData*>(handleData);
        Token->callbackData.Reset(CreateCallbackData(callback, receiver));
      }

      TokenData() {}

      Persistent<Object> callbackData;
      std::function<void ()> func;

      friend Async;
    };

  public:
    template<typename TInput, typename TResult> 
    static void __cdecl Run(
      std::shared_ptr<TInput> input, 
      std::function<void (Baton<TInput, TResult>*)> doWork, 
      std::function<void (Baton<TInput, TResult>*)> afterWork, 
      Handle<Function> callback,
      Handle<Value> receiver = Handle<Value>())
    {
      HandleScope scope;
      Local<Object> callbackData = CreateCallbackData(callback, receiver);
      
      Baton<TInput, TResult>* baton = new Baton<TInput, TResult>();
      baton->request.data = baton;
      baton->callbackData.Reset(callbackData);
      baton->error_code = 0;
      baton->data = input;
      baton->doWork = doWork;
      baton->afterWork = afterWork;

      uv_queue_work(uv_default_loop(), &baton->request, AsyncWork<TInput, TResult>, AsyncAfter<TInput, TResult>);
    }

    static uv_async_t* __cdecl GetAsyncToken()
    {
      return TokenData::NewAsyncToken();
    }

    static uv_async_t* __cdecl GetAsyncToken(
      Handle<Function> callback, 
      Handle<Value> receiver = Handle<Value>())
    {
      return TokenData::NewAsyncToken(callback, receiver);
    }

    static uv_idle_t* __cdecl GetIdleToken()
    {
      return TokenData::NewIdleToken();
    }

    static uv_idle_t* __cdecl GetIdleToken(
      Handle<Function> callback, 
      Handle<Value> receiver = Handle<Value>())
    {
      return TokenData::NewIdleToken(callback, receiver);
    }

    static void __cdecl RunOnMain(uv_async_t* async, std::function<void ()> func)
    {
      TokenData* Token = static_cast<TokenData*>(async->data);
      Token->func = func;
      uv_async_send(async);
    }

    static void __cdecl RunOnMain(std::function<void ()> func)
    {
      static unsigned int uvMainThreadId = GetMainThreadId();
      
      if (uvMainThreadId == (unsigned int)uv_thread_self()) 
      {
        func();
      }
      else
      {
        uv_async_t *async = GetAsyncToken();
        RunOnMain(async, func);
      }
    }

    static void __cdecl RunCallbackOnMain(
      uv_async_t* async,
      std::function<void (InvokeCallbackDelegate invokeCallback)> func)
    {
      TokenData* Token = static_cast<TokenData*>(async->data);

      InvokeCallbackDelegate invokeCallback = [Token](int argc, Handle<Value>* argv)
      {
        if (!Token->callbackData.IsEmpty())
        {
          MakeCallback(New(Token->callbackData), New<String>("callback").ToLocalChecked(), argc, argv);
        }
      };

      std::function<void ()> wrapper = [func, invokeCallback]()
      {
        HandleScope scope;
        func(invokeCallback);
      };

      RunOnMain(async, wrapper);
    }

    // defers execution of the provided function by creating an idler
    // that means, the function will be invoked once the event loop has delivered
    // all pending events.
    static void __cdecl NextTick(std::function<void ()> func)
    {
      uv_idle_t *idler = GetIdleToken();
      NextTick(idler, func);
    }

    static void __cdecl NextTick(uv_idle_t *idler, std::function<void ()> func)
    {
      TokenData* Token = static_cast<TokenData*>(idler->data);
      Token->func = func;

      uv_idle_start(idler, onNextTick);
    }

    static void __cdecl RunCallbackOnNextTick(
      uv_idle_t* idler,
      std::function<void (InvokeCallbackDelegate invokeCallback)> func)
    {
      TokenData* Token = static_cast<TokenData*>(idler->data);

      InvokeCallbackDelegate invokeCallback = [Token](int argc, Handle<Value>* argv)
      {
        if (!Token->callbackData.IsEmpty())
        {
          MakeCallback(New(Token->callbackData), New<String>("callback").ToLocalChecked(), argc, argv);
        }
      };

      std::function<void ()> wrapper = [func, invokeCallback]()
      {
        HandleScope scope;
        func(invokeCallback);
      };

      NextTick(idler, wrapper);
    }

  private:
    static Handle<Object> CreateCallbackData(Handle<Function> callback, Handle<Value> receiver)
    {
      EscapableHandleScope scope;

      Local<Object> callbackData;
      if (!callback.IsEmpty() && !callback->Equals(Undefined()))
      {
        callbackData = New<Object>();
        
        if (!receiver.IsEmpty())
        {
          callbackData->SetPrototype(receiver);
        }
        
        callbackData->Set(New<String>("callback").ToLocalChecked(), callback);
      
        // get the current domain:
        Handle<Value> currentDomain = Undefined();

        Handle<Object> process = GetCurrentContext()->Global()->Get(New<String>("process").ToLocalChecked()).As<Object>();
        if (!process->Equals(Undefined()))
        {
          currentDomain = process->Get(New<String>("domain").ToLocalChecked()) ;
        }

        callbackData->Set(New<String>("domain").ToLocalChecked(), currentDomain);
      }

      return scope.Escape(callbackData);
    };

    template<typename TInput, typename TResult> 
    static void __cdecl AsyncWork(uv_work_t* req) 
    {
      // No HandleScope!

      Baton<TInput, TResult>* baton = static_cast<Baton<TInput, TResult>*>(req->data);

      // Do work in threadpool here.
      // Set baton->error_code/message on failures.
      // Set baton->result with a final result object
      baton->doWork(baton);
    }


    template<typename TInput, typename TResult> 
    static void __cdecl AsyncAfter(uv_work_t* req, int status) 
    {
      HandleScope scope;;
      Baton<TInput, TResult>* baton = static_cast<Baton<TInput, TResult>*>(req->data);

      // typical AfterWorkFunc implementation
      //if (baton->error) 
      //{
      //  Handle<Value> err = Exception::Error(...);
      //  Handle<Value> argv[] = { err };
      //  baton->setCallbackArgs(argv, _countof(argv));
      //}
      //else
      //{
      //  Handle<Value> argv[] = { Undefined(), ... };
      //  baton->setCallbackArgs(argv, _countof(argv));
      //} 

      baton->afterWork(baton);
      
      if (!baton->callbackData.IsEmpty())
      {
        // call the callback, using domains and all
        int argc = static_cast<int>(baton->callback_args_size);
        std::unique_ptr<Local<Value>> handlesArr(new Local<Value>[argc]);
        for (int i=0; i < argc; i++)
        {
          handlesArr.get()[i] = New(baton->callback_info.get()[i]);
        }

        MakeCallback(New(baton->callbackData), New<String>("callback").ToLocalChecked(), argc, handlesArr.get());
      }
      
      baton->callbackData.Reset();
      delete baton;
    }
    
    // called after the async handle is closed in order to free it's memory
    static void __cdecl AyncCloseCb(uv_handle_t* handle) 
    {
      if (handle != nullptr)
      {
        uv_async_t* async = reinterpret_cast<uv_async_t*>(handle);
        delete async;
      }
    }

    // Called by run on main in case we are not running on the main thread
    NODEASYNC_ASYNC_WORK_CB(AsyncCb)
    {
      auto Token = static_cast<TokenData*>(handle->data);
      Token->func();
      uv_close((uv_handle_t*)handle, AyncCloseCb);
      delete Token;
    }

    // Attributes goes to http://stackoverflow.com/a/1982200/1060807 (etan)
    static unsigned int __cdecl GetMainThreadId()
    {
      const std::shared_ptr<void> hThreadSnapshot(
        CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0), CloseHandle);

      if (hThreadSnapshot.get() == INVALID_HANDLE_VALUE)
      {
        return 0;
      }

      THREADENTRY32 tEntry;
      tEntry.dwSize = sizeof(THREADENTRY32);
      DWORD result = 0;
      DWORD currentPID = GetCurrentProcessId();

      for (BOOL success = Thread32First(hThreadSnapshot.get(), &tEntry);
        !result && success && GetLastError() != ERROR_NO_MORE_FILES;
        success = Thread32Next(hThreadSnapshot.get(), &tEntry))
      {
        if (tEntry.th32OwnerProcessID == currentPID) 
        {
          result = tEntry.th32ThreadID;
        }
      }
      return result;
    }

    NODEASYNC_IDLE_WORK_CB(onNextTick)
    {
      std::function<void ()> *func = static_cast<std::function<void ()> *>(handle->data);
      (*func)();
      delete func;
      uv_idle_stop(handle);
      delete handle;
    }
  };
}

