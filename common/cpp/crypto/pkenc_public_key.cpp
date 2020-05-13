/* Copyright 2018 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file
 * Avalon RSA public key generation, serialization, and encryption functions.
 */

#include <openssl/err.h>
#include <openssl/pem.h>
#include <memory>    // std::unique_ptr

#include "crypto_shared.h"
#include "error.h"
#include "hex_string.h"
#include "pkenc.h"
#include "pkenc_private_key.h"
#include "pkenc_public_key.h"

namespace pcrypto = tcf::crypto;
namespace constants = tcf::crypto::constants;

// Typedefs for memory management.
// Specify type and destroy function type for unique_ptr.
typedef std::unique_ptr<BIO, void (*)(BIO*)> BIO_ptr;
typedef std::unique_ptr<EVP_CIPHER_CTX, void (*)(EVP_CIPHER_CTX*)> CTX_ptr;
typedef std::unique_ptr<BN_CTX, void (*)(BN_CTX*)> BN_CTX_ptr;
typedef std::unique_ptr<BIGNUM, void (*)(BIGNUM*)> BIGNUM_ptr;
typedef std::unique_ptr<RSA, void (*)(RSA*)> RSA_ptr;

// Error handling
namespace Error = tcf::error;


/**
 * Utility function: deserialize RSA Public Key.
 * Throws RuntimeError, ValueError.
 *
 * @param encoded Serialized RSA public key to deserialize
 */
RSA* deserializeRSAPublicKey(const std::string& encoded) {
    BIO_ptr bio(BIO_new_mem_buf(encoded.c_str(), -1), BIO_free_all);
    if (!bio) {
        std::string msg(
            "Crypto Error (deserializeRSAPublicKey): Could not create BIO");
        throw Error::RuntimeError(msg);
    }

    RSA* public_key = PEM_read_bio_RSAPublicKey(bio.get(), NULL, NULL, NULL);
    if (!public_key) {
        std::string msg(
            "Crypto Error (deserializeRSAPublicKey): Could not "
            "deserialize public RSA key");
        throw Error::ValueError(msg);
    }
    return public_key;
}  // deserializeRSAPublicKey


/**
 * PublicKey constructor.
 */
pcrypto::pkenc::PublicKey::PublicKey() {
    public_key_ = nullptr;
}  // pcrypto::sig::PublicKey::PublicKey


/**
 * PublicKey constructor from PrivateKey.
 * Extracts the public key portion from a RSA key pair.
 */
pcrypto::pkenc::PublicKey::PublicKey(
        const pcrypto::pkenc::PrivateKey& privateKey) {
    public_key_ = RSAPublicKey_dup(privateKey.private_key_);
    if (!public_key_) {
        std::string msg("Crypto  Error (pkenc::PublicKey()): "
            "Could not duplicate RSA public key");
        throw Error::RuntimeError(msg);
    }
}  // pcrypto::pkenc::PublicKey::PublicKey


/**
 * Constructor from encoded string.
 * Implemented with deserializeRSAPublicKey().
 * Throws RuntimeError, ValueError.
 *
 * @param encoded serialized RSA public key
 */
pcrypto::pkenc::PublicKey::PublicKey(const std::string& encoded) {
    public_key_ = deserializeRSAPublicKey(encoded);
}  // pcrypto::pkenc::PublicKey::PublicKey


/**
 * PublicKey destructor.
 */
pcrypto::pkenc::PublicKey::~PublicKey() {
    if (public_key_)
        RSA_free(public_key_);
}  // pcrypto::pkenc::Public::~PublicKey


/**
 * Copy constructor.
 * Throws RuntimeError.
 */
pcrypto::pkenc::PublicKey::PublicKey(
        const pcrypto::pkenc::PublicKey& publicKey) {
    public_key_ = RSAPublicKey_dup(publicKey.public_key_);
    if (!public_key_) {
        std::string msg("Crypto Error (pkenc::PublicKey() copy): "
            "Could not copy public key");
        throw Error::RuntimeError(msg);
    }
}  // pcrypto::pkenc::PublicKey::PublicKey (copy constructor)


/**
 * Move constructor.
 * Throws RuntimeError.
 */
pcrypto::pkenc::PublicKey::PublicKey(pcrypto::pkenc::PublicKey&& publicKey) {
    public_key_ = publicKey.public_key_;
    publicKey.public_key_ = nullptr;
    if (!public_key_) {
        std::string msg("Crypto Error (pkenc::PublicKey() move): "
            "Cannot move null public key");
        throw Error::RuntimeError(msg);
    }
}  // pcrypto::pkenc::PublicKey::PublicKey (move constructor)


/**
 * Assignment operator = overload.
 * Throws RuntimeError.
 */
pcrypto::pkenc::PublicKey& pcrypto::pkenc::PublicKey::operator=(
    const pcrypto::pkenc::PublicKey& publicKey) {
    if (this == &publicKey)
        return *this;
    if (public_key_)
        RSA_free(public_key_);
    public_key_ = RSAPublicKey_dup(publicKey.public_key_);
    if (!public_key_) {
        std::string msg("Crypto Error (pkenc::PublicKey::operator =): "
            "Could not copy public key");
        throw Error::RuntimeError(msg);
    }
    return *this;
}  // pcrypto::pkenc::PublicKey::operator =


/**
 * Deserialize Public Key.
 * Implemented with deserializeRSAPublicKey().
 * Throws RuntimeError, ValueError.
 *
 * @param encoded Serialized RSA public key to deserialize
 */
void pcrypto::pkenc::PublicKey::Deserialize(const std::string& encoded) {
    RSA* key = deserializeRSAPublicKey(encoded);
    if (public_key_)
        RSA_free(public_key_);
    public_key_ = key;
}  // pcrypto::pkenc::PublicKey::Deserialize


/**
 * Serialize a RSA public key.
 * Throws RuntimeError.
 */
std::string pcrypto::pkenc::PublicKey::Serialize() const {
    if (public_key_ == nullptr) {
        std::string msg(
            "Crypto Error (Serialize): PublicKey is not initialized");
        throw Error::RuntimeError(msg);
    }

    BIO_ptr bio(BIO_new(BIO_s_mem()), BIO_free_all);
    if (!bio) {
        std::string msg("Crypto Error (Serialize): Could not create BIO");
        throw Error::RuntimeError(msg);
    }

    int res = PEM_write_bio_RSAPublicKey(bio.get(), public_key_);
    if (!res) {
        std::string msg("Crypto Error (Serialize): Could not write to BIO");
        throw Error::RuntimeError(msg);
    }

    int keylen = BIO_pending(bio.get());
    ByteArray pem_str(keylen + 1);

    res = BIO_read(bio.get(), pem_str.data(), keylen);
    if (!res) {
        std::string msg("Crypto Error (Serialize): Could not red BIO");
        throw Error::RuntimeError(msg);
    }

    pem_str[keylen] = '\0';
    std::string str(reinterpret_cast<char*>(pem_str.data()));
    return str;
}  // pcrypto::pkenc::PublicKey::Serialize


/**
 * Encrypt message with RSA public key and return ciphertext.
 * Uses PKCS1 OAEP padding.
 * Throws RuntimeError.
 *
 * @param message ByteArray containing raw binary plaintext
 * @returns ByteArray containing raw binary ciphertext
 */
ByteArray pcrypto::pkenc::PublicKey::EncryptMessage(const ByteArray& message)
        const {
    char err[constants::ERR_BUF_LEN];
    int ctext_len;

    if (message.size() == 0) {
        std::string msg(
            "Crypto Error (EncryptMessage): RSA plaintext cannot be empty");
        throw Error::RuntimeError(msg);
    }

    if (message.size() > constants::RSA_PLAINTEXT_LEN) {
        std::string msg(
            "Crypto Error (EncryptMessage): RSA plaintext size is too large");
        throw Error::RuntimeError(msg);
    }

    ByteArray ctext(RSA_size(public_key_));
    ctext_len = RSA_public_encrypt(message.size(), message.data(),
        ctext.data(), public_key_, constants::RSA_PADDING_SCHEME);

    if (ctext_len == -1) {
        std::string msg("Crypto Error (EncryptMessage): "
            "RSA encryption internal error.\n");
        ERR_load_crypto_strings();
        ERR_error_string(ERR_get_error(), err);
        msg += err;
        throw Error::RuntimeError(msg);
    }

    return ctext;
}  // pcrypto::pkenc::PublicKey::EncryptMessage
