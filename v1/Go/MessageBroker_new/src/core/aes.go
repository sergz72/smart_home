package core

import (
  "bytes"
  "compress/gzip"
  "crypto/aes"
  "crypto/cipher"
  "crypto/rand"
  "fmt"
  "io"
)

func GenerateRandomBytes(size int) ([]byte, error) {
  b := make([]byte, size)

  if _, err := io.ReadFull(rand.Reader, b); err != nil {
    return nil, err
  }

  return b, nil
}

func zipData(data []byte) ([]byte, error) {
  var buf bytes.Buffer
  zw := gzip.NewWriter(&buf)
  _, err := zw.Write(data)
  if err != nil {
    return nil, err
  }
  if err := zw.Close(); err != nil {
    return nil, err
  }

  return buf.Bytes(), nil
}

func unzipData(data []byte) ([]byte, error) {
  buf := bytes.NewBuffer(data)
  zr, err := gzip.NewReader(buf)
  if err != nil {
    return nil, err
  }
  var outbuf bytes.Buffer
  _, err = outbuf.ReadFrom(zr)
  if err != nil {
    return nil, err
  }
  return outbuf.Bytes(), nil
}

func AesEncode(data []byte, key []byte, zip bool, buildNonce func(int) ([]byte, error)) ([]byte, error) {
  var err error
  if zip {
    data, err = zipData(data)
    if err != nil {
      return nil, err
    }
  }
  block, err := aes.NewCipher(key)
  if err != nil {
    return nil, err
  }
  gcm, err := cipher.NewGCM(block)
  if err != nil {
    return nil, err
  }
  var nonce []byte
  if buildNonce == nil {
    nonce, err = GenerateRandomBytes(gcm.NonceSize())
  } else {
    nonce, err = buildNonce(gcm.NonceSize())
  }
  if err != nil {
    return nil, err
  }
  return gcm.Seal(nonce, nonce, data, nil), nil
}

func AesDecode(data []byte, key []byte, zip bool, checkNonce func([]byte) error) ([]byte, error) {
  block, err := aes.NewCipher(key)
  if err != nil {
    return nil, err
  }
  gcm, err := cipher.NewGCM(block)
  if err != nil {
    return nil, err
  }
  nonceSize := gcm.NonceSize()
  if len(data) < nonceSize {
    return nil, fmt.Errorf("too short data length")
  }
  nonce, encryptedData := data[:nonceSize], data[nonceSize:]
  if checkNonce != nil {
    err = checkNonce(nonce)
    if err != nil {
      return nil, err
    }
  }
  decryptedData, err := gcm.Open(nil, nonce, encryptedData, nil)
  if err != nil {
    return nil, err
  }
  if zip {
    decryptedData, err = unzipData(decryptedData)
    if err != nil {
      return nil, err
    }
  }

  return decryptedData, nil
}
