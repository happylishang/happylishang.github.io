序列化Parcel

Parcel不能盲解，Client跟Server端必须知道彼此的顺序才能解析，或者将Parcel看做是入栈与出栈操作，只能这样才能解析，不能说你想读什么，就能读什么，想按照什么顺序就按照什么顺序，它并不支持随机访问，类似于链表。


尽量避免使用Parcel的writeSerializable

    /**
     * Write a generic serializable object in to a Parcel.  It is strongly
     * recommended that this method be avoided, since the serialization
     * overhead is extremely large, and this approach will be much slower than
     * using the other approaches to writing data in to a Parcel.
     */
    public final void writeSerializable(Serializable s) {
        if (s == null) {
            writeString(null);
            return;
        }
        String name = s.getClass().getName();
        writeString(name);

        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        try {
            ObjectOutputStream oos = new ObjectOutputStream(baos);
            oos.writeObject(s);
            oos.close();

            writeByteArray(baos.toByteArray());
        } catch (IOException ioe) {
            throw new RuntimeException("Parcelable encountered " +
                "IOException writing serializable object (name = " + name +
                ")", ioe);
        }
    }
    

对于一些普通的字段会辗转调用writeFieldValues，利用反射获取每个字段的值，这样会增加运行负担
    
        private void writeFieldValues(EmulatedFieldsForDumping emulatedFields) throws IOException {
        // Access internal fields which we can set/get. Users can't do this.
        EmulatedFields accessibleSimulatedFields = emulatedFields.emulatedFields();
        for (EmulatedFields.ObjectSlot slot : accessibleSimulatedFields.slots()) {
            Object fieldValue = slot.getFieldValue();
            Class<?> type = slot.getField().getType();
            if (type == int.class) {
                output.writeInt(fieldValue != null ? ((Integer) fieldValue).intValue() : 0);
            } else if (type == byte.class) {
                output.writeByte(fieldValue != null ? ((Byte) fieldValue).byteValue() : 0);
            } else if (type == char.class) {
                output.writeChar(fieldValue != null ? ((Character) fieldValue).charValue() : 0);
            } else if (type == short.class) {
                output.writeShort(fieldValue != null ? ((Short) fieldValue).shortValue() : 0);
            } else if (type == boolean.class) {
                output.writeBoolean(fieldValue != null ? ((Boolean) fieldValue).booleanValue() : false);
            } else if (type == long.class) {
                output.writeLong(fieldValue != null ? ((Long) fieldValue).longValue() : 0);
            } else if (type == float.class) {
                output.writeFloat(fieldValue != null ? ((Float) fieldValue).floatValue() : 0);
            } else if (type == double.class) {
                output.writeDouble(fieldValue != null ? ((Double) fieldValue).doubleValue() : 0);
            } else {
                // Either array or Object
                writeObject(fieldValue);
            }
        }
    }