---

layout: post
title: Android APK V1 V2签名与验证原理简析
category: Android

---

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

![X.509证书格式](https://upload-images.jianshu.io/upload_images/1460468-bec878a4c3bf6049.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

从CERT.RSA中，我们能获的证书的指纹信息，在微信分享、第三方SDK申请的时候经常用到，其实就是公钥+开发者信息的一个签名：

![image.png](https://upload-images.jianshu.io/upload_images/1460468-6be76e3c90bb4548.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

除了CERT.RSA文件，其余两个签名文件其实跟keystore没什么关系，主要是文件自身的摘要及二次摘要，用不同的keystore进行签名，生成的MANIFEST.MF与CERT.SF都是一样的，不同的只有CERT.RSA签名文件。也就是说前两者主要保证各个文件的完整性，CERT.RSA从整体上保证APK的完整性，不过META_INF中文件不在校验范围中，这也是V1的一个缺点。V2签名又是如何保证信息的完整性呢？

> V2签名块如何保证APK的完整性

前面说过V1签名中文件的完整性很容易被绕过，可以理解**单个文件完整性校验的意义并不是很大**，安装的时候反而耗时，不如采用更加简单的便捷的校验方式。V2签名就不针对单个文件校验了，而是**针对APK进行校验**，将APK分成1M的块，对每个块计算值摘要，之后针对所有摘要进行摘要，再利用摘要进行签名。

![image.png](https://upload-images.jianshu.io/upload_images/1460468-2b927ae5b3833a76.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)


也就是说，V2摘要签名分两级，第一级是对APK文件的1、3 、4 部分进行摘要，第二级是对第一级的摘要集合进行摘要，然后利用秘钥进行签名。安装的时候，块摘要可以并行处理，这样可以提高校验速度。

## 简单的APK签名流程（签名原理）

APK是先摘要，再签名，先看下摘要的定义：Message Digest：摘要是对消息数据执行一个单向Hash，从而生成一个固定长度的Hash值，这个值就是消息摘要，至于常听到的MD5、SHA1都是摘要算法的一种。理论上说，摘要一定会有碰撞，但只要保证有限长度内碰撞率很低就可以，这样就能利用摘要来保证消息的完整性，只要消息被篡改，摘要一定会发生改变。但是，如果消息跟摘要同时被修改，那就无从得知了。

而数字签名是什么呢（公钥数字签名），利用非对称加密技术，通过私钥对摘要进行加密，产生一个字符串，这个字符串+公钥证书就可以看做消息的数字签名，如RSA就是常用的非对称加密算法。在没有私钥的前提下，非对称加密算法能确保别人无法伪造签名，因此数字签名也是对发送者信息真实性的一个有效证明。不过由于Android的keystore证书是自签名的，没有第三方权威机构认证，用户可以自行生成keystore，Android签名方案无法保证APK不被二次签名。 

知道了摘要跟签名的概念后，再来看看Android的签名文件怎么来的？如何影响原来APK包？通过sdk中的apksign来对一个APK进行签名的命令如下：

	 ./apksigner sign  --ks   keystore.jks  --ks-key-alias keystore  --ks-pass pass:XXX  --key-pass pass:XXX  --out output.apk input.apk

其主要实现在 android/platform/tools/apksig 文件夹中，主体是ApkSigner.java的sign函数，函数比较长，分几步分析

    private void sign(
            DataSource inputApk,
            DataSink outputApkOut,
            DataSource outputApkIn)
                    throws IOException, ApkFormatException, NoSuchAlgorithmException,
                            InvalidKeyException, SignatureException {
        // Step 1. Find input APK's main ZIP sections
        ApkUtils.ZipSections inputZipSections;
        <!--根据zip包的结构，找到APK中包内容Object-->
        try {
            inputZipSections = ApkUtils.findZipSections(inputApk);
        ...

先来看这一步，ApkUtils.findZipSections，这个函数主要是解析APK文件，获得ZIP格式的一些简单信息，并返回一个ZipSections，

	 public static ZipSections findZipSections(DataSource apk)
	            throws IOException, ZipFormatException {
	        Pair<ByteBuffer, Long> eocdAndOffsetInFile =
	                ZipUtils.findZipEndOfCentralDirectoryRecord(apk);
	        ByteBuffer eocdBuf = eocdAndOffsetInFile.getFirst();
	        long eocdOffset = eocdAndOffsetInFile.getSecond();
	        eocdBuf.order(ByteOrder.LITTLE_ENDIAN);
	        long cdStartOffset = ZipUtils.getZipEocdCentralDirectoryOffset(eocdBuf);
	        ...
	        long cdSizeBytes = ZipUtils.getZipEocdCentralDirectorySizeBytes(eocdBuf);
	        long cdEndOffset = cdStartOffset + cdSizeBytes;
	        int cdRecordCount = ZipUtils.getZipEocdCentralDirectoryTotalRecordCount(eocdBuf);
	        return new ZipSections(
	                cdStartOffset,
	                cdSizeBytes,
	                cdRecordCount,
	                eocdOffset,
	                eocdBuf);
	    }

ZipSections包含了ZIP文件格式的一些信息，比如中央目录信息、中央目录结尾信息等，对比到zip文件格式如下：
  
  ![image.png](https://upload-images.jianshu.io/upload_images/1460468-d7a88a4842691598.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)      
        
获取到 ZipSections之后，就可以进一步解析APK这个ZIP包，继续走后面的签名流程，
        
        long inputApkSigningBlockOffset = -1;
        DataSource inputApkSigningBlock = null;
        <!--检查V2签名是否存在-->
        try {
            Pair<DataSource, Long> apkSigningBlockAndOffset =
                    V2SchemeVerifier.findApkSigningBlock(inputApk, inputZipSections);
            inputApkSigningBlock = apkSigningBlockAndOffset.getFirst();
            inputApkSigningBlockOffset = apkSigningBlockAndOffset.getSecond();
        } catch (V2SchemeVerifier.SignatureNotFoundException e) {
        <!--V2签名不存在也没什么问题，非必须-->
    }
     <!--获取V2签名以外的信息区域-->
     DataSource inputApkLfhSection =
                inputApk.slice(
                        0,
                        (inputApkSigningBlockOffset != -1)
                                ? inputApkSigningBlockOffset
                                : inputZipSections.getZipCentralDirectoryOffset());
可以看到先进行了一个V2签名的检验，这里是用来签名，为什么先检验了一次？第一次签名的时候会直接走这个异常逻辑分支，重复签名的时候才能获到取之前的V2签名，怀疑这里获取V2签名的目的应该是为了排除V2签名，并获取V2签名以外的数据块，因为签名本身不能被算入到签名中，之后会解析中央目录区，构建一个DefaultApkSignerEngine用于签名
      
          <!--解析中央目录区，目的是为了解析AndroidManifest-->
        // Step 2. Parse the input APK's ZIP Central Directory
        ByteBuffer inputCd = getZipCentralDirectory(inputApk, inputZipSections);
        List<CentralDirectoryRecord> inputCdRecords =
                parseZipCentralDirectory(inputCd, inputZipSections);

        // Step 3. Obtain a signer engine instance
        ApkSignerEngine signerEngine;
        if (mSignerEngine != null) {
            signerEngine = mSignerEngine;
        } else {
            // Construct a signer engine from the provided parameters
            ...
            List<DefaultApkSignerEngine.SignerConfig> engineSignerConfigs =
                    new ArrayList<>(mSignerConfigs.size());
            <!--一般就一个-->
            for (SignerConfig signerConfig : mSignerConfigs) {
                engineSignerConfigs.add(
                        new DefaultApkSignerEngine.SignerConfig.Builder(
                                signerConfig.getName(),
                                signerConfig.getPrivateKey(),
                                signerConfig.getCertificates())
                                .build());
            }
            <!--默认V1 V2都启用-->
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

先解析中央目录区，获取AndroidManifest文件，获取minSdkVersion(影响签名算法)，并构建DefaultApkSignerEngine，默认情况下V1 V2签名都是打开的。


        // Step 4. Provide the signer engine with the input APK's APK Signing Block (if any)
        <!--忽略这一步-->
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
        ...

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
        
第五步与第六步的主要工作是：apk的预处理，包括目录的一些排序之类的工作，应该是为了更高效处理签名，预处理结束后，就开始签名流程，首先做的是V1签名（默认存在，除非主动关闭）：

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


步骤7、8、9都可以看做是V1签名的处理逻辑，主要在V1SchemeSigner中处理，其中包括创建META-INFO文件夹下的一些签名文件，更新中央目录、更新中央目录结尾等，流程不复杂，不在赘述，简单流程就是：

![image.png](https://upload-images.jianshu.io/upload_images/1460468-fe38ec5e1979d120.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

这里特殊提一下重复签名的问题：**对一个已经V1签名的APK再次V1签名不会有任何问题**，原理就是：再次签名的时候，会排除之前的签名文件。

	  public static boolean isJarEntryDigestNeededInManifest(String entryName) {
	        // See https://docs.oracle.com/javase/8/docs/technotes/guides/jar/jar.html#Signed_JAR_File
	
	        // Entries which represent directories sould not be listed in the manifest.
	        if (entryName.endsWith("/")) {
	            return false;
	        }
	
	        // Entries outside of META-INF must be listed in the manifest.
	        if (!entryName.startsWith("META-INF/")) {
	            return true;
	        }
	        // Entries in subdirectories of META-INF must be listed in the manifest.
	        if (entryName.indexOf('/', "META-INF/".length()) != -1) {
	            return true;
	        }
	
	        // Ignored file names (case-insensitive) in META-INF directory:
	        //   MANIFEST.MF
	        //   *.SF
	        //   *.RSA
	        //   *.DSA
	        //   *.EC
	        //   SIG-*
	        String fileNameLowerCase =
	                entryName.substring("META-INF/".length()).toLowerCase(Locale.US);
	        if (("manifest.mf".equals(fileNameLowerCase))
	                || (fileNameLowerCase.endsWith(".sf"))
	                || (fileNameLowerCase.endsWith(".rsa"))
	                || (fileNameLowerCase.endsWith(".dsa"))
	                || (fileNameLowerCase.endsWith(".ec"))
	                || (fileNameLowerCase.startsWith("sig-"))) {
	            return false;
	        }
	        return true;
	    }
	    
可以看到目录、META-INF文件夹下的文件、sf、rsa等结尾的文件都不会被V1签名进行处理，所以这里不用担心多次签名的问题。接下来就是处理V2签名。

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
    
V2SchemeSigner处理V2签名，逻辑比较清晰，直接对V1签名过的APK进行分块摘要，再集合签名，V2签名不会改变之前V1签名后的任何信息，签名后，在中央目录前添加V2签名块，并更新中央目录结尾信息，因为V2签名后，中央目录的偏移会再次改变：

![image.png](https://upload-images.jianshu.io/upload_images/1460468-f9b4c4d44ab1e29a.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

# APK签名怎么校验

签名校验的过程可以看做签名的逆向，只不过覆盖安装可能还要校验公钥及证书信息一致，否则覆盖安装会失败。签名校验的入口在PackageManagerService的install里，安装官方文档，7.0以上的手机优先检测V2签名，如果V2签名不存在，再校验V1签名，对于7.0以下的手机，不存在V2签名校验机制，只会校验V1，所以，如果你的App的miniSdkVersion<24(N)，那么你的签名方式必须内含V1签名：

![签名校验流程](https://upload-images.jianshu.io/upload_images/1460468-061357d5da6b5daa.png?imageMogr2/auto-orient/strip%7CimageView2/2/w/1240)

校验流程就是签名的逆向，了解签名流程即可，本文不求甚解，有兴趣自己去分析，只是额外提下覆盖安装，覆盖安装除了检验APK自己的完整性以外，还要校验证书是否一致只有证书一致（同一个keystore签名），才有可能覆盖升级。覆盖安装同全新安装相比较多了几个校验

* 包名一致
* 证书一致
* versioncode不能降低

这里只关心证书部分：

        // Verify: if target already has an installer package, it must
        // be signed with the same cert as the caller.
        if (targetPackageSetting.installerPackageName != null) {
            PackageSetting setting = mSettings.mPackages.get(
                    targetPackageSetting.installerPackageName);
            // If the currently set package isn't valid, then it's always
            // okay to change it.
            if (setting != null) {
                if (compareSignatures(callerSignature,
                        setting.signatures.mSignatures)
                        != PackageManager.SIGNATURE_MATCH) {
                    throw new SecurityException(
                            "Caller does not have same cert as old installer package "
                            + targetPackageSetting.installerPackageName);
                }
            }
        }

# V1、V2签名下美团多渠道打包的切入点

* V1签名：META_INFO文件夹下增加文件不会对校验有任何影响，则是美团V1多渠道打包方案的切入点
* V2签名：V2签名块中可以添加一些附属信息，不会对签名又任何影响，这是V2多渠道打包的切入点。
 
# 总结

* V1签名靠META_INFO文件夹下的签名文件
* V2签名依靠中央目录前的V2签名快，ZIP的目录结构不会改变，当然结尾偏移要改。
* V1 V2签名可以同时存在（7.0以下如果没有V1签名是不可以的）
* 多去到打包的切入点原则：附加信息不影响签名验证
 
# 参考文档

[Android签名与认证详细分析之一（CERT.RSA剖析）     
[Android签名与认证详细分析之二（CERT.RSA剖析）](https://blog.csdn.net/xiqingnian/article/details/28316767)          
](https://blog.csdn.net/xiqingnian/article/details/27338677)               
[Android中签名原理和安全性分析之META-INF文件讲解](http://www.chenglong.ren/2016/12/30/android%E4%B8%AD%E7%AD%BE%E5%90%8D%E5%8E%9F%E7%90%86%E5%92%8C%E5%AE%89%E5%85%A8%E6%80%A7%E5%88%86%E6%9E%90%E4%B9%8Bmeta-inf%E6%96%87%E4%BB%B6%E8%AE%B2%E8%A7%A3/)
[Android应用程序签名过程分析](https://blog.csdn.net/roland_sun/article/details/41825791)         
[Android签名验证原理解析](https://blog.csdn.net/hp910315/article/details/77684725)     