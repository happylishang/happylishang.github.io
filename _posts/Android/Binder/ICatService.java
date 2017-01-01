/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Original file: /Users/personal/prj/crazy_Android/10/10.2/AidlClient/src/org/crazyit/service/ICatService.aidl
 */
package org.crazyit.service;

public interface ICatService extends android.os.IInterface {
    /** Local-side IPC implementation stub class. */
    public static abstract class Stub extends android.os.Binder implements org.crazyit.service.ICatService {
        private static final java.lang.String DESCRIPTOR = "org.crazyit.service.ICatService";

        /** Construct the stub at attach it to the interface. */
        public Stub() {
            this.attachInterface(this, DESCRIPTOR);
        }

        /**
         * Cast an IBinder object into an org.crazyit.service.ICatService
         * interface, generating a proxy if needed.
         */
        public static org.crazyit.service.ICatService asInterface(android.os.IBinder obj) {
            if ((obj == null)) {
                return null;
            }
            android.os.IInterface iin = obj.queryLocalInterface(DESCRIPTOR);
            if (((iin != null) && (iin instanceof org.crazyit.service.ICatService))) {
                return ((org.crazyit.service.ICatService) iin);
            }
            return new org.crazyit.service.ICatService.Stub.Proxy(obj);
        }

        @Override
        public android.os.IBinder asBinder() {
            return this;
        }

        @Override
        public boolean onTransact(int code, android.os.Parcel data, android.os.Parcel reply, int flags)
                throws android.os.RemoteException {
            switch (code) {
            case INTERFACE_TRANSACTION: {
                reply.writeString(DESCRIPTOR);
                return true;
            }
            case TRANSACTION_getColor: {
                data.enforceInterface(DESCRIPTOR);
                java.lang.String _result = this.getColor();
                reply.writeNoException();
                reply.writeString(_result);
                return true;
            }
            case TRANSACTION_getWeight: {
                data.enforceInterface(DESCRIPTOR);
                double _result = this.getWeight();
                reply.writeNoException();
                reply.writeDouble(_result);
                return true;
            }
            }
            return super.onTransact(code, data, reply, flags);
        }

        private static class Proxy implements org.crazyit.service.ICatService {
            private android.os.IBinder mRemote;

            Proxy(android.os.IBinder remote) {
                mRemote = remote;
            }

            @Override
            public android.os.IBinder asBinder() {
                return mRemote;
            }

            public java.lang.String getInterfaceDescriptor() {
                return DESCRIPTOR;
            }

            @Override
            public java.lang.String getColor() throws android.os.RemoteException {
                android.os.Parcel _data = android.os.Parcel.obtain();
                android.os.Parcel _reply = android.os.Parcel.obtain();
                java.lang.String _result;
                try {
                    _data.writeInterfaceToken(DESCRIPTOR);
                    mRemote.transact(Stub.TRANSACTION_getColor, _data, _reply, 0);
                    _reply.readException();
                    _result = _reply.readString();
                } finally {
                    _reply.recycle();
                    _data.recycle();
                }
                return _result;
            }

            @Override
            public double getWeight() throws android.os.RemoteException {
                android.os.Parcel _data = android.os.Parcel.obtain();
                android.os.Parcel _reply = android.os.Parcel.obtain();
                double _result;
                try {
                    _data.writeInterfaceToken(DESCRIPTOR);
                    mRemote.transact(Stub.TRANSACTION_getWeight, _data, _reply, 0);
                    _reply.readException();
                    _result = _reply.readDouble();
                } finally {
                    _reply.recycle();
                    _data.recycle();
                }
                return _result;
            }
        }

        static final int TRANSACTION_getColor = (android.os.IBinder.FIRST_CALL_TRANSACTION + 0);
        static final int TRANSACTION_getWeight = (android.os.IBinder.FIRST_CALL_TRANSACTION + 1);
    }

    public java.lang.String getColor() throws android.os.RemoteException;

    public double getWeight() throws android.os.RemoteException;
}
