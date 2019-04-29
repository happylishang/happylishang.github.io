Android为了保证系统及应用的安全性，在安装APK的时候需要校验包的完整性，同时，对于覆盖安装的场景还要校验新旧是否匹配，这两者都是通过Android签名机制来进行保证的，本文就简单看下Android的签名与校验原理，分一下几个部分分析下：

* APK签名是什么
* APK签名如何保证APK信息完整性
* 如何为APK签名
* APK签名怎么校验

# Android的APK签名是什么

签名是摘要与非对称密钥加密相相结合的产物，签名就像内容的一个指纹信息，一旦内容被篡改，就其原来签名就会失效，以此来校验信息的完整性。APK签名也是这个道理，如果APK签名跟内容对应不起来，Android系统就认为APK内容被篡改了，从而拒绝安装，以保证系统的安全性。目前Android有三种签名V1、V2（N）、V3（P），本文只看前两种V1跟V2，对于V3的轮秘先不考虑，先看下只有V1签名后APK的样式：

![image.png](https://upload-images.jianshu.io/upload_images/1460468-a71a02b26a0918c7.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

再看下只有V2签名的APK包样式：

![image.png](https://upload-images.jianshu.io/upload_images/1460468-80a2075573d65726.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

同时具有V1 V2签名：

![image.png](https://upload-images.jianshu.io/upload_images/1460468-4dcd0acdcf5097c6.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

可以看到，如果只有V2签名，那么APK包内容几乎是没有改动的，META_INF中不会有新增文件，按Google官方文档：在使用v2签名方案进行签名时，会在APK文件中插入一个APK签名分块，该分块位于zip中央目录部分之前并紧邻该部分。在APK签名分块内，**签名和签名者身份信息会存储在APK签名方案v2分块中，保证整个APK文件不可修改**，如下图：
 
 ![image.png](https://upload-images.jianshu.io/upload_images/1460468-d78df27d385aa547.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)
 
而V1签名是通过META-INF中的三个文件保证签名及信息的完整性：

![image.png](https://upload-images.jianshu.io/upload_images/1460468-d7f391f36849b755.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

# APK签名如何保证APK信息完整性

V1签名是如何保证信息的完整性呢？V1签名主要包含三部分内容，如果狭义上说签名跟公钥的话，仅仅在.rsa文件中，V1签名的三个文件其实是一套机制，不能单单拿一个来说事，

> MANIFEST.MF：摘要文件，**存储文件名与文件SHA1摘要（Base64格式）键值对**，格式如下，其主要作用是**保证每个文件的完整性**

![摘要](https://upload-images.jianshu.io/upload_images/1460468-9c5659a1e9350cbc.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

如果对APK中的资源文件进行了替换，那么该资源的摘要必定发生改变，如果没有修改MANIFEST.MF中的信息，那么在安装时候V1校验就会失败，无法安装，不过如果篡改文件的同时，也修改其MANIFEST.MF中的摘要值，那么MANIFEST.MF校验就可以绕过。

>CERT.SF：二次摘要文件，存储文件名与**MANIFEST.MF摘要条目的SHA1摘要**（Base64格式）键值对，格式如下

![image.png](https://upload-images.jianshu.io/upload_images/1460468-fe0797043b305ccf.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

CERT.SF个人觉得有点像冗余，更像对文件完整性的二次保证，同绕过MANIFEST.MF一样，.SF校验也很容易被绕过。

>CERT.RSA 证书（公钥）及签名文件，存储keystore的公钥、发行信息、以及对CERT.SF文件摘要的签名信息（利用keystore的**私钥**进行加密过）

CERT.RSA与CERT.SF是相互对应的，两者名字前缀必须一致，不知道算不算一个无聊的标准。看下CERT.RSA文件内容：

![image.png](https://upload-images.jianshu.io/upload_images/1460468-eec4220db5304fda.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

CERT.RSA文件里面存储了证书公钥、过期日期、发行人、加密算法等信息，根据公钥及加密算法，Android系统就能计算出CERT.SF的摘要信息，其严格的格式如下：

![X.509证书格式](https://img-blog.csdn.net/20140603223556046?watermark/2/text/aHR0cDovL2Jsb2cuY3Nkbi5uZXQvWERfbGl4aW4=/font/5a6L5L2T/fontsize/400/fill/I0JBQkFCMA==/dissolve/70/gravity/SouthEast)

从CERT.RSA中，我们能获的证书的指纹信息，在微信分享、第三方SDK申请的时候经常用到，其实就是公钥+开发者信息的一个签名：

![image.png](https://upload-images.jianshu.io/upload_images/1460468-6be76e3c90bb4548.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

除了CERT.RSA文件，其余两个签名文件其实跟keystore没什么关系，主要是文件自身的摘要及二次摘要，用不同的keystore进行签名，生成的MANIFEST.MF与CERT.SF都是一样的，不同的只有CERT.RSA签名文件。也就是说前两者主要保证各个文件的完整性，CERT.RSA从整体上保证APK的完整性，不过META_INF中文件不在校验范围中，这也是V1的一个缺点。V2签名又是如何保证信息的完整性呢？

> V2签名块如何保证APK的完整性

前面说过V1签名中文件的完整性很容易被绕过，可以理解**单个文件完整性校验的意义并不是很大**，安装的时候反而耗时，不如采用更加简单的便捷的校验方式。V2签名就不针对单个文件校验了，而是**针对APK进行校验**，将APK分成1M的块，对每个块计算值摘要，之后针对所有摘要进行摘要，再利用摘要进行签名。

![image.png](https://upload-images.jianshu.io/upload_images/1460468-2b927ae5b3833a76.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


也就是说，V2摘要签名分两级，第一级是对APK文件的1、3 、4 部分进行摘要，第二级是对第一级的摘要集合进行摘要，然后利用秘钥进行签名。安装的时候，块摘要可以并行处理，这样可以提高校验速度。

## 怎么为APK签名（签名原理）

APK是先摘要，再签名，先看下摘要的定义：Message Digest：摘要是对消息数据执行一个单向Hash，从而生成一个固定长度的Hash值，这个值就是消息摘要，至于常听到的MD5、SHA1都是摘要算法的一种。理论上说，摘要一定会有碰撞，但只要保证有限长度内碰撞率很低就可以，这样就能利用摘要来保证消息的完整性，只要消息被篡改，摘要一定会发生改变。但是，如果消息跟摘要同时被修改，那就无从得知了。

而数字签名是什么呢（公钥数字签名），利用非对称加密技术，通过私钥对摘要进行加密，产生一个字符串，这个字符串+公钥证书就可以看做消息的数字签名，如RSA就是常用的非对称加密算法。在没有私钥的前提下，非对称加密算法能确保别人无法伪造签名，因此数字签名也是对发送者信息真实性的一个有效证明。不过由于Android的keystore证书是自签名的，没有第三方权威机构认证，用户可以自行生成keystore，Android签名方案无法保证APK不被二次签名。 

Android的签名文件怎么来的？如何影响原来APK包？我们可以通过sdk中的apksign来对一个APK进行签名：

	 ./apksigner sign  --ks   keystore.jks  --ks-key-alias keystore  --ks-pass pass:XXX  --key-pass pass:XXX  --out output.apk input.apk

无论APK是否签名，都没问题，**重复签名也不会有什么问题**。因为之前的签名无论是否存在，都不会被计算到本次签名中，apksigner的原理是什么呢？主要实现在 android/platform/tools/apksig 文件夹中，主题是ApkSignerTool.java的sign函数：

    private void sign(
            DataSource inputApk,
            DataSink outputApkOut,
            DataSource outputApkIn)
                    throws IOException, ApkFormatException, NoSuchAlgorithmException,
                            InvalidKeyException, SignatureException {
        // Step 1. Find input APK's main ZIP sections
        ApkUtils.ZipSections inputZipSections;
        try {
            inputZipSections = ApkUtils.findZipSections(inputApk);
        } catch (ZipFormatException e) {
            throw new ApkFormatException("Malformed APK: not a ZIP archive", e);
        }
        long inputApkSigningBlockOffset = -1;
        DataSource inputApkSigningBlock = null;
        try {
            Pair<DataSource, Long> apkSigningBlockAndOffset =
                    V2SchemeVerifier.findApkSigningBlock(inputApk, inputZipSections);
            inputApkSigningBlock = apkSigningBlockAndOffset.getFirst();
            inputApkSigningBlockOffset = apkSigningBlockAndOffset.getSecond();
        } catch (V2SchemeVerifier.SignatureNotFoundException e) {
            // Input APK does not contain an APK Signing Block. That's OK. APKs are not required to
            // contain this block. It's only needed if the APK is signed using APK Signature Scheme
            // v2.
        }
        DataSource inputApkLfhSection =
                inputApk.slice(
                        0,
                        (inputApkSigningBlockOffset != -1)
                                ? inputApkSigningBlockOffset
                                : inputZipSections.getZipCentralDirectoryOffset());

        // Step 2. Parse the input APK's ZIP Central Directory
        ByteBuffer inputCd = getZipCentralDirectory(inputApk, inputZipSections);
        List<CentralDirectoryRecord> inputCdRecords =
                parseZipCentralDirectory(inputCd, inputZipSections);

        // Step 3. Obtain a signer engine instance
        ApkSignerEngine signerEngine;
        if (mSignerEngine != null) {
            // Use the provided signer engine
            signerEngine = mSignerEngine;
        } else {
            // Construct a signer engine from the provided parameters
            int minSdkVersion;
            if (mMinSdkVersion != null) {
                // No need to extract minSdkVersion from the APK's AndroidManifest.xml
                minSdkVersion = mMinSdkVersion;
            } else {
                // Need to extract minSdkVersion from the APK's AndroidManifest.xml
                minSdkVersion = getMinSdkVersionFromApk(inputCdRecords, inputApkLfhSection);
            }
            List<DefaultApkSignerEngine.SignerConfig> engineSignerConfigs =
                    new ArrayList<>(mSignerConfigs.size());
            for (SignerConfig signerConfig : mSignerConfigs) {
                engineSignerConfigs.add(
                        new DefaultApkSignerEngine.SignerConfig.Builder(
                                signerConfig.getName(),
                                signerConfig.getPrivateKey(),
                                signerConfig.getCertificates())
                                .build());
            }
            DefaultApkSignerEngine.Builder signerEngineBuilder =
                    new DefaultApkSignerEngine.Builder(engineSignerConfigs, minSdkVersion)
                            .setV1SigningEnabled(mV1SigningEnabled)
                            .setV2SigningEnabled(mV2SigningEnabled)
                            .setOtherSignersSignaturesPreserved(mOtherSignersSignaturesPreserved);
            if (mCreatedBy != null) {
                signerEngineBuilder.setCreatedBy(mCreatedBy);
            }
            signerEngine = signerEngineBuilder.build();
        }

        // Step 4. Provide the signer engine with the input APK's APK Signing Block (if any)
        if (inputApkSigningBlock != null) {
            signerEngine.inputApkSigningBlock(inputApkSigningBlock);
        }

        // Step 5. Iterate over input APK's entries and output the Local File Header + data of those
        // entries which need to be output. Entries are iterated in the order in which their Local
        // File Header records are stored in the file. This is to achieve better data locality in
        // case Central Directory entries are in the wrong order.
        List<CentralDirectoryRecord> inputCdRecordsSortedByLfhOffset =
                new ArrayList<>(inputCdRecords);
        Collections.sort(
                inputCdRecordsSortedByLfhOffset,
                CentralDirectoryRecord.BY_LOCAL_FILE_HEADER_OFFSET_COMPARATOR);
        int lastModifiedDateForNewEntries = -1;
        int lastModifiedTimeForNewEntries = -1;
        long inputOffset = 0;
        long outputOffset = 0;
        Map<String, CentralDirectoryRecord> outputCdRecordsByName =
                new HashMap<>(inputCdRecords.size());
        for (final CentralDirectoryRecord inputCdRecord : inputCdRecordsSortedByLfhOffset) {
            String entryName = inputCdRecord.getName();
            ApkSignerEngine.InputJarEntryInstructions entryInstructions =
                    signerEngine.inputJarEntry(entryName);
            boolean shouldOutput;
            switch (entryInstructions.getOutputPolicy()) {
                case OUTPUT:
                    shouldOutput = true;
                    break;
                case OUTPUT_BY_ENGINE:
                case SKIP:
                    shouldOutput = false;
                    break;
                default:
                    throw new RuntimeException(
                            "Unknown output policy: " + entryInstructions.getOutputPolicy());
            }

            long inputLocalFileHeaderStartOffset = inputCdRecord.getLocalFileHeaderOffset();
            if (inputLocalFileHeaderStartOffset > inputOffset) {
                // Unprocessed data in input starting at inputOffset and ending and the start of
                // this record's LFH. We output this data verbatim because this signer is supposed
                // to preserve as much of input as possible.
                long chunkSize = inputLocalFileHeaderStartOffset - inputOffset;
                inputApkLfhSection.feed(inputOffset, chunkSize, outputApkOut);
                outputOffset += chunkSize;
                inputOffset = inputLocalFileHeaderStartOffset;
            }
            LocalFileRecord inputLocalFileRecord;
            try {
                inputLocalFileRecord =
                        LocalFileRecord.getRecord(
                                inputApkLfhSection, inputCdRecord, inputApkLfhSection.size());
            } catch (ZipFormatException e) {
                throw new ApkFormatException("Malformed ZIP entry: " + inputCdRecord.getName(), e);
            }
            inputOffset += inputLocalFileRecord.getSize();

            ApkSignerEngine.InspectJarEntryRequest inspectEntryRequest =
                    entryInstructions.getInspectJarEntryRequest();
            if (inspectEntryRequest != null) {
                fulfillInspectInputJarEntryRequest(
                        inputApkLfhSection, inputLocalFileRecord, inspectEntryRequest);
            }

            if (shouldOutput) {
                // Find the max value of last modified, to be used for new entries added by the
                // signer.
                int lastModifiedDate = inputCdRecord.getLastModificationDate();
                int lastModifiedTime = inputCdRecord.getLastModificationTime();
                if ((lastModifiedDateForNewEntries == -1)
                        || (lastModifiedDate > lastModifiedDateForNewEntries)
                        || ((lastModifiedDate == lastModifiedDateForNewEntries)
                                && (lastModifiedTime > lastModifiedTimeForNewEntries))) {
                    lastModifiedDateForNewEntries = lastModifiedDate;
                    lastModifiedTimeForNewEntries = lastModifiedTime;
                }

                inspectEntryRequest = signerEngine.outputJarEntry(entryName);
                if (inspectEntryRequest != null) {
                    fulfillInspectInputJarEntryRequest(
                            inputApkLfhSection, inputLocalFileRecord, inspectEntryRequest);
                }

                // Output entry's Local File Header + data
                long outputLocalFileHeaderOffset = outputOffset;
                long outputLocalFileRecordSize =
                        outputInputJarEntryLfhRecordPreservingDataAlignment(
                                inputApkLfhSection,
                                inputLocalFileRecord,
                                outputApkOut,
                                outputLocalFileHeaderOffset);
                outputOffset += outputLocalFileRecordSize;

                // Enqueue entry's Central Directory record for output
                CentralDirectoryRecord outputCdRecord;
                if (outputLocalFileHeaderOffset == inputLocalFileRecord.getStartOffsetInArchive()) {
                    outputCdRecord = inputCdRecord;
                } else {
                    outputCdRecord =
                            inputCdRecord.createWithModifiedLocalFileHeaderOffset(
                                    outputLocalFileHeaderOffset);
                }
                outputCdRecordsByName.put(entryName, outputCdRecord);
            }
        }
        long inputLfhSectionSize = inputApkLfhSection.size();
        if (inputOffset < inputLfhSectionSize) {
            // Unprocessed data in input starting at inputOffset and ending and the end of the input
            // APK's LFH section. We output this data verbatim because this signer is supposed
            // to preserve as much of input as possible.
            long chunkSize = inputLfhSectionSize - inputOffset;
            inputApkLfhSection.feed(inputOffset, chunkSize, outputApkOut);
            outputOffset += chunkSize;
            inputOffset = inputLfhSectionSize;
        }

        // Step 6. Sort output APK's Central Directory records in the order in which they should
        // appear in the output
        List<CentralDirectoryRecord> outputCdRecords = new ArrayList<>(inputCdRecords.size() + 10);
        for (CentralDirectoryRecord inputCdRecord : inputCdRecords) {
            String entryName = inputCdRecord.getName();
            CentralDirectoryRecord outputCdRecord = outputCdRecordsByName.get(entryName);
            if (outputCdRecord != null) {
                outputCdRecords.add(outputCdRecord);
            }
        }

        // Step 7. Generate and output JAR signatures, if necessary. This may output more Local File
        // Header + data entries and add to the list of output Central Directory records.
        ApkSignerEngine.OutputJarSignatureRequest outputJarSignatureRequest =
                signerEngine.outputJarEntries();
        if (outputJarSignatureRequest != null) {
            if (lastModifiedDateForNewEntries == -1) {
                lastModifiedDateForNewEntries = 0x3a21; // Jan 1 2009 (DOS)
                lastModifiedTimeForNewEntries = 0;
            }
            for (ApkSignerEngine.OutputJarSignatureRequest.JarEntry entry :
                    outputJarSignatureRequest.getAdditionalJarEntries()) {
                String entryName = entry.getName();
                byte[] uncompressedData = entry.getData();
                ZipUtils.DeflateResult deflateResult =
                        ZipUtils.deflate(ByteBuffer.wrap(uncompressedData));
                byte[] compressedData = deflateResult.output;
                long uncompressedDataCrc32 = deflateResult.inputCrc32;

                ApkSignerEngine.InspectJarEntryRequest inspectEntryRequest =
                        signerEngine.outputJarEntry(entryName);
                if (inspectEntryRequest != null) {
                    inspectEntryRequest.getDataSink().consume(
                            uncompressedData, 0, uncompressedData.length);
                    inspectEntryRequest.done();
                }

                long localFileHeaderOffset = outputOffset;
                outputOffset +=
                        LocalFileRecord.outputRecordWithDeflateCompressedData(
                                entryName,
                                lastModifiedTimeForNewEntries,
                                lastModifiedDateForNewEntries,
                                compressedData,
                                uncompressedDataCrc32,
                                uncompressedData.length,
                                outputApkOut);


                outputCdRecords.add(
                        CentralDirectoryRecord.createWithDeflateCompressedData(
                                entryName,
                                lastModifiedTimeForNewEntries,
                                lastModifiedDateForNewEntries,
                                uncompressedDataCrc32,
                                compressedData.length,
                                uncompressedData.length,
                                localFileHeaderOffset));
            }
            outputJarSignatureRequest.done();
        }

        // Step 8. Construct output ZIP Central Directory in an in-memory buffer
        long outputCentralDirSizeBytes = 0;
        for (CentralDirectoryRecord record : outputCdRecords) {
            outputCentralDirSizeBytes += record.getSize();
        }
        if (outputCentralDirSizeBytes > Integer.MAX_VALUE) {
            throw new IOException(
                    "Output ZIP Central Directory too large: " + outputCentralDirSizeBytes
                            + " bytes");
        }
        ByteBuffer outputCentralDir = ByteBuffer.allocate((int) outputCentralDirSizeBytes);
        for (CentralDirectoryRecord record : outputCdRecords) {
            record.copyTo(outputCentralDir);
        }
        outputCentralDir.flip();
        DataSource outputCentralDirDataSource = new ByteBufferDataSource(outputCentralDir);
        long outputCentralDirStartOffset = outputOffset;
        int outputCentralDirRecordCount = outputCdRecords.size();

        // Step 9. Construct output ZIP End of Central Directory record in an in-memory buffer
        ByteBuffer outputEocd =
                EocdRecord.createWithModifiedCentralDirectoryInfo(
                        inputZipSections.getZipEndOfCentralDirectory(),
                        outputCentralDirRecordCount,
                        outputCentralDirDataSource.size(),
                        outputCentralDirStartOffset);

        // Step 10. Generate and output APK Signature Scheme v2 signatures, if necessary. This may
        // insert an APK Signing Block just before the output's ZIP Central Directory
        ApkSignerEngine.OutputApkSigningBlockRequest outputApkSigingBlockRequest =
                signerEngine.outputZipSections(
                        outputApkIn,
                        outputCentralDirDataSource,
                        DataSources.asDataSource(outputEocd));
        if (outputApkSigingBlockRequest != null) {
            byte[] outputApkSigningBlock = outputApkSigingBlockRequest.getApkSigningBlock();
            outputApkOut.consume(outputApkSigningBlock, 0, outputApkSigningBlock.length);
            ZipUtils.setZipEocdCentralDirectoryOffset(
                    outputEocd, outputCentralDirStartOffset + outputApkSigningBlock.length);
            outputApkSigingBlockRequest.done();
        }

        // Step 11. Output ZIP Central Directory and ZIP End of Central Directory
        outputCentralDirDataSource.feed(0, outputCentralDirDataSource.size(), outputApkOut);
        outputApkOut.consume(outputEocd);
        signerEngine.outputDone();
    }
 
 
所有有关apk文件的签名验证工作都是在JarVerifier里面做的，一共分成三步；
JarVeirifer.verifyCertificate主要做了两步。首先，使用证书文件（在META-INF目录下，以.DSA、.RSA或者.EC结尾的文件）检验签名文件（在META-INF目录下，和证书文件同名，但扩展名为.SF的文件）是没有被修改过的。然后，使用签名文件，检验MANIFEST.MF文件中的内容也没有被篡改过；
JarVerifier.VerifierEntry.verify做了最后一步验证，即保证apk文件中包含的所有文件，对应的摘要值与MANIFEST.MF文件中记录的一致。
 

# 覆盖安装校验(没有私钥就很难获得与公钥想对应的正确签名)

覆盖安装同全新安装相比较多了两个校验

* 包名一致
* 签名一致：实际说的就是公钥需要一致

也就是说必须使用同一个keystore签名，否则APK无法覆盖安装，当然也要满足versioncode不能降低。

假如我们是一个非法者，想要篡改apk内容，我们怎么做呢？如果我们只把原文件改动了（比如加入了自己的病毒代码），那么重新打包后系统就会认为文件的SHA1-Base64值和MF的不一致导致安装失败，既然这样，那我们就改一下MF让他们一致呗？如果只是这样那么系统就会发现MF文件的内容的SHA1-Base64与SF不一致，还是会安装失败，既然这样，那我们就改一下SF和MF一致呗？如果这么做了，系统就会发现RSA解密后的值和SF的SHA1不一致，安装失败。那么我们让加密后的值和SF的SHA1一致就好了呗，但是呢，这个用来签名加密的是私钥，公钥随便玩，但是私钥我们却没有，所以没法做到一致。所以说上面的过程环环相扣，最后指向了RSA非对称加密的保证。有人说，那我可以直接重签名啊，这样所有的信息就一致了啊，是的，没错，重签名后就可以安装了，这就是说签名机制只是保证了apk的完整性，具体是不是自己的apk包，系统并不知道，那我们上面说的安全性是怎么保证的呢？那就是我们可以随便签名，随便安装，但是在覆盖安装的时候由于我们的签名和作者的签名不一致，导致我们重签名后的apk无法覆盖掉原作者的。这就保证了已经安装的apk的接下来的安全链的正确性。当然了，如果你的手机上来就直接安装了一个第三方的非法签名的apk，那么原作者的官方apk也不能再安装了，因为系统认为他是非法的。

说明：在这一步，即使开发者修改了程序内容，并生成了新的摘要文件，MANIFEST.MF能与内容对应起来，CERT.SF也能与内容对应起来，但是攻击者没有开发者的私钥，所以不能生成正确的签名文件（CERT.RSA）。系统在对程序进行验证的时候，用开发者公钥对不正确的签名文件进行解密，得到的结果对应不起来，所以不能通过检验，不能成功安装文件（覆盖安装），如果完全用新的签名自己签名一遍，全新安装时没问题的。 

## 美团Walle多渠道打包支持V2的原理

不修改APK的内容，但是修改偏移，不修改META内容，不更改zip的内容目录只是修改了签名块，第一代打包则要全部修改。

# 参考文档

[Android签名与认证详细分析之一（CERT.RSA剖析）     
[Android签名与认证详细分析之二（CERT.RSA剖析）](https://blog.csdn.net/xiqingnian/article/details/28316767)          
](https://blog.csdn.net/xiqingnian/article/details/27338677)               
[Android中签名原理和安全性分析之META-INF文件讲解](http://www.chenglong.ren/2016/12/30/android%E4%B8%AD%E7%AD%BE%E5%90%8D%E5%8E%9F%E7%90%86%E5%92%8C%E5%AE%89%E5%85%A8%E6%80%A7%E5%88%86%E6%9E%90%E4%B9%8Bmeta-inf%E6%96%87%E4%BB%B6%E8%AE%B2%E8%A7%A3/)
[Android应用程序签名过程分析](https://blog.csdn.net/roland_sun/article/details/41825791)         
[Android签名验证原理解析](https://blog.csdn.net/hp910315/article/details/77684725)     