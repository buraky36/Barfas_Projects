#ifndef CRYPTO_MANAGER_H
#define CRYPTO_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// AES-256-GCM settings
#define CRYPTO_AES_KEY_SIZE 32
#define CRYPTO_AES_IV_SIZE 12
#define CRYPTO_AES_TAG_SIZE 16

// X25519 & Ed25519 sizes
#define CRYPTO_X25519_KEY_SIZE 32
#define CRYPTO_ED25519_PUBKEY_SIZE 32
#define CRYPTO_ED25519_SIG_SIZE 64

// Local key size
#define CRYPTO_LOCAL_KEY_SIZE 32
#define CRYPTO_NONCE_SIZE 12

/**
 * @brief Generate cryptographically secure random bytes
 */
bool crypto_generate_random(uint8_t *output, size_t length);

/**
 * @brief Initialize a new X25519 keypair for ephemeral ECDH
 * @param priv_key Buffer to store 32-byte private key
 * @param pub_key Buffer to store 32-byte public key
 * @return true on success
 */
bool crypto_generate_x25519_keypair(uint8_t *priv_key, uint8_t *pub_key);

/**
 * @brief Calculate ECDH shared secret using our private key and peer's public key
 * @param my_priv 32-byte X25519 private key
 * @param peer_pub 32-byte X25519 public key
 * @param shared_secret Buffer to store 32-byte shared secret
 * @return true on success
 */
bool crypto_ecdh_shared_secret(const uint8_t *my_priv, const uint8_t *peer_pub, uint8_t *shared_secret);

/**
 * @brief Derive a session key using HKDF-SHA256
 * @param ikm Input key material (e.g. ECDH shared secret)
 * @param ikm_len Length of IKM
 * @param salt Salt buffer (e.g. connNonce)
 * @param salt_len Length of salt
 * @param info Info string (e.g. "onloi-gw-session-v1")
 * @param info_len Length of info
 * @param okm Output key material (e.g. 32-byte AES key)
 * @param okm_len Length of OKM to derive
 * @return true on success
 */
bool crypto_hkdf_sha256(const uint8_t *ikm, size_t ikm_len,
                        const uint8_t *salt, size_t salt_len,
                        const uint8_t *info, size_t info_len,
                        uint8_t *okm, size_t okm_len);

/**
 * @brief AES-256-GCM Encryption
 * @param key 32-byte AES key
 * @param iv 12-byte initialization vector (nonce)
 * @param plaintext Data to encrypt
 * @param plaintext_len Length of data
 * @param aad Additional Authenticated Data (optional)
 * @param aad_len Length of AAD
 * @param ciphertext Buffer for output ciphertext (same length as plaintext)
 * @param tag 16-byte buffer for auth tag
 * @return true on success
 */
bool crypto_aes_gcm_encrypt(const uint8_t *key, const uint8_t *iv,
                            const uint8_t *plaintext, size_t plaintext_len,
                            const uint8_t *aad, size_t aad_len,
                            uint8_t *ciphertext, uint8_t *tag);

/**
 * @brief AES-256-GCM Decryption
 * @param key 32-byte AES key
 * @param iv 12-byte initialization vector (nonce)
 * @param ciphertext Data to decrypt
 * @param ciphertext_len Length of data
 * @param aad Additional Authenticated Data (optional)
 * @param aad_len Length of AAD
 * @param tag 16-byte auth tag to verify
 * @param plaintext Buffer for output plaintext (same length as ciphertext)
 * @return true on success
 */
bool crypto_aes_gcm_decrypt(const uint8_t *key, const uint8_t *iv,
                            const uint8_t *ciphertext, size_t ciphertext_len,
                            const uint8_t *aad, size_t aad_len,
                            const uint8_t *tag,
                            uint8_t *plaintext);

/**
 * @brief Calculate HMAC-SHA256
 * @param key HMAC key
 * @param key_len Key length
 * @param data Data to hash
 * @param data_len Data length
 * @param mac Output 32-byte MAC buffer
 * @return true on success
 */
bool crypto_hmac_sha256(const uint8_t *key, size_t key_len,
                        const uint8_t *data, size_t data_len,
                        uint8_t *mac);

/**
 * @brief Verify an Ed25519 signature
 * @param pubkey 32-byte public key
 * @param msg Data that was signed (e.g. SHA256 of firmware)
 * @param msg_len Length of data
 * @param sig 64-byte signature
 * @return true if signature is valid
 */
bool crypto_ed25519_verify(const uint8_t *pubkey, const uint8_t *msg, size_t msg_len, const uint8_t *sig);

/**
 * @brief Calculate SHA256 hash
 * @param data Data to hash
 * @param data_len Length of data
 * @param hash Output 32-byte hash buffer
 * @return true on success
 */
bool crypto_sha256(const uint8_t *data, size_t data_len, uint8_t *hash);

#ifdef __cplusplus
}
#endif

#endif // CRYPTO_MANAGER_H
