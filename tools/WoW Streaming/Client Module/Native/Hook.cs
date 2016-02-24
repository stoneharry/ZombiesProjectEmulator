using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace WoW.Streaming.Native
{
    abstract class Hook
    {
        protected static readonly List<Hook> ActiveHooks = new List<Hook>();

        protected abstract void OnUnload();

        public static void UnloadAll()
        {
            lock (ActiveHooks)
                ActiveHooks.ForEach(h => h.OnUnload());
        }
    }

    class Hook<T> : Hook, IDisposable
    {
        private readonly Delegate mOriginal;
        private T mHook; // we need a reference to the hook to avoid GC collecting it when its bound
        private readonly IntPtr mTarget;
        private readonly bool mIsRelative;
        private bool mIsDisposed;

        private readonly byte[] mOrigCode = new byte[6];
        private readonly byte[] mNewCode = new byte[6];

        public Hook(IntPtr address, bool relative, T hook)
        {
            mIsRelative = relative;
            mTarget = address;
            mHook = hook;
            var methodPtr = Marshal.GetFunctionPointerForDelegate((Delegate)(object) hook);
            mNewCode[0] = 0x68;
            Buffer.BlockCopy(BitConverter.GetBytes(methodPtr.ToInt32()), 0, mNewCode, 1, 4);
            mNewCode[5] = 0xC3;

            if (mIsRelative)
                Memory.ReadRelative(mTarget, mOrigCode);
            else
                Memory.Read(mTarget, mOrigCode);

            mOriginal = Marshal.GetDelegateForFunctionPointer(mIsRelative ? (mTarget + Memory.BaseAddress.ToInt32()) : mTarget, typeof (T));

            lock (ActiveHooks)
                ActiveHooks.Add(this);
        }

        protected override void OnUnload()
        {
            lock(this)
            {
                mIsDisposed = true;
                Disable();
            }

        }

        public void Dispose()
        {
            lock (ActiveHooks)
                ActiveHooks.RemoveAll(h => h == this);
        }

        public void Apply()
        {
            if (mIsDisposed)
                return;

            if (mIsRelative)
                Memory.WriteRelative(mTarget, mNewCode);
            else
                Memory.Write(mTarget, mNewCode);
        }

        public void Disable()
        {
            if (mIsRelative)
                Memory.WriteRelative(mTarget, mOrigCode);
            else
                Memory.Write(mTarget, mOrigCode);
        }

        public object CallOriginal(params object[] args)
        {
            Disable();
            var ret = mOriginal.DynamicInvoke(args);
            Apply();
            return ret;
        }
    }
}
