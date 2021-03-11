package core

import (
  "bytes"
  "compress/bzip2"
  "compress/gzip"
  "crypto/aes"
  "crypto/cipher"
  "crypto/rand"
  "fmt"
  "io"
)

const (
  CompressNone = iota
  CompressGzip = iota
  CompressBzip = iota
)

func GetCompressionType(compressionTypeString string) (int, error) {
  switch compressionTypeString {
  case "gz": return CompressGzip, nil
  case "bz": return CompressBzip, nil
  default: return 0, fmt.Errorf("unknown compression type")
  }
}

func GenerateRandomBytes(size int) ([]byte, error) {
  b := make([]byte, size)

  if _, err := io.ReadFull(rand.Reader, b); err != nil {
    return nil, err
  }

  return b, nil
}

func zipData(data []byte) ([]byte, error) {
  var buf bytes.Buffer
  zw, err := gzip.NewWriterLevel(&buf, gzip.BestCompression)
  if err != nil {
    return nil, err
  }
  _, err = zw.Write(data)
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

func bunzipData(data []byte) ([]byte, error) {
  if len(data) <= 74 {
    return data, nil
  }
  buf := bytes.NewBuffer(data)
  zr := bzip2.NewReader(buf)
  var outbuf bytes.Buffer
  _, err := outbuf.ReadFrom(zr)
  if err != nil {
    return nil, err
  }
  return outbuf.Bytes(), nil
}

func compressData(zip int, data []byte) ([]byte, error) {
  switch zip {
  case CompressGzip: return zipData(data)
  case CompressBzip: return bzipData(data)
  default: return data, nil
  }
}

func decompressData(zip int, data []byte) ([]byte, error) {
  switch zip {
  case CompressGzip: return unzipData(data)
  case CompressBzip: return bunzipData(data)
  default: return data, nil
  }
}

func AesEncode(data []byte, key []byte, zip int, buildNonce func(int) ([]byte, error)) ([]byte, error) {
  var err error
  data, err = compressData(zip, data)
  if err != nil {
      return nil, err
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

func AesDecode(data []byte, key []byte, zip int, checkNonce func([]byte) error) ([]byte, error) {
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
  decryptedData, err = decompressData(zip, decryptedData)
  if err != nil {
      return nil, err
    }

  return decryptedData, nil
}
