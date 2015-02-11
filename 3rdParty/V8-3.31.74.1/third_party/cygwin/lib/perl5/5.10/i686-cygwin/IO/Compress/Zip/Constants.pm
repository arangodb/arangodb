package IO::Compress::Zip::Constants;

use strict ;
use warnings;

require Exporter;

our ($VERSION, @ISA, @EXPORT, %ZIP_CM_MIN_VERSIONS);

$VERSION = '2.011';

@ISA = qw(Exporter);

@EXPORT= qw(

    ZIP_CM_STORE
    ZIP_CM_DEFLATE
    ZIP_CM_BZIP2
    ZIP_CM_LZMA
    ZIP_CM_PPMD
    
    ZIP_LOCAL_HDR_SIG
    ZIP_DATA_HDR_SIG
    ZIP_CENTRAL_HDR_SIG
    ZIP_END_CENTRAL_HDR_SIG
    ZIP64_END_CENTRAL_REC_HDR_SIG
    ZIP64_END_CENTRAL_LOC_HDR_SIG
    ZIP64_ARCHIVE_EXTRA_SIG
    ZIP64_DIGITAL_SIGNATURE_SIG

    ZIP_GP_FLAG_ENCRYPTED_MASK
    ZIP_GP_FLAG_STREAMING_MASK
    ZIP_GP_FLAG_PATCHED_MASK
    ZIP_GP_FLAG_STRONG_ENCRYPTED_MASK
    ZIP_GP_FLAG_LZMA_EOS_PRESENT
    ZIP_GP_FLAG_LANGUAGE_ENCODING

    ZIP_EXTRA_ID_ZIP64
    ZIP_EXTRA_ID_EXT_TIMESTAMP
    ZIP_EXTRA_ID_INFO_ZIP_UNIX2

    ZIP_OS_CODE_UNIX
    ZIP_OS_CODE_DEFAULT

    ZIP_IFA_TEXT_MASK

    %ZIP_CM_MIN_VERSIONS
    ZIP64_MIN_VERSION

    );

# Compression types supported
use constant ZIP_CM_STORE                      => 0 ;
use constant ZIP_CM_DEFLATE                    => 8 ;
use constant ZIP_CM_BZIP2                      => 12 ;
use constant ZIP_CM_LZMA                       => 14 ; # Not Supported yet
use constant ZIP_CM_PPMD                       => 98 ; # Not Supported yet

# General Purpose Flag
use constant ZIP_GP_FLAG_ENCRYPTED_MASK        => (1 << 0) ;
use constant ZIP_GP_FLAG_STREAMING_MASK        => (1 << 3) ;
use constant ZIP_GP_FLAG_PATCHED_MASK          => (1 << 5) ;
use constant ZIP_GP_FLAG_STRONG_ENCRYPTED_MASK => (1 << 6) ;
use constant ZIP_GP_FLAG_LZMA_EOS_PRESENT      => (1 << 1) ;
use constant ZIP_GP_FLAG_LANGUAGE_ENCODING     => (1 << 11) ;

# Internal File Attributes
use constant ZIP_IFA_TEXT_MASK                 => 1;

# Signatures for each of the headers
use constant ZIP_LOCAL_HDR_SIG                 => 0x04034b50;
use constant ZIP_DATA_HDR_SIG                  => 0x08074b50;
use constant ZIP_CENTRAL_HDR_SIG               => 0x02014b50;
use constant ZIP_END_CENTRAL_HDR_SIG           => 0x06054b50;
use constant ZIP64_END_CENTRAL_REC_HDR_SIG     => 0x06064b50;
use constant ZIP64_END_CENTRAL_LOC_HDR_SIG     => 0x07064b50;
use constant ZIP64_ARCHIVE_EXTRA_SIG           => 0x08064b50;
use constant ZIP64_DIGITAL_SIGNATURE_SIG       => 0x05054b50;

use constant ZIP_OS_CODE_UNIX                  => 3;
use constant ZIP_OS_CODE_DEFAULT               => 3;

# Extra Field ID's
use constant ZIP_EXTRA_ID_ZIP64                => pack "v", 1;
use constant ZIP_EXTRA_ID_EXT_TIMESTAMP        => "UT";
use constant ZIP_EXTRA_ID_INFO_ZIP_UNIX2       => "Ux";
use constant ZIP_EXTRA_ID_JAVA_EXE             => pack "v", 0xCAFE;

use constant ZIP64_MIN_VERSION                 => 45;

%ZIP_CM_MIN_VERSIONS = (
            ZIP_CM_STORE()                      => 20,
            ZIP_CM_DEFLATE()                    => 20,
            ZIP_CM_BZIP2()                      => 46,
            ZIP_CM_LZMA()                       => 63,
            );


1;

__END__

