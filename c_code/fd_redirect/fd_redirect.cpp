#include <iostream>
#include <atomic>
#include <thread>
#include <mutex>
#include <functional>

#include <stdio.h>
#include <unistd.h>
#include <string.h>

using namespace std;

/**
  * Signature for redirected FD output functions
  * Functions of this type are expected to take a single FD as an input
  * parameter. The FD is meant to be read-only, and the function should
  * exit gracefully when read() returns a non-positive number (i.e. <= 0).
  */
using FnFDOutput = std::function<void(const int)>;

/**
 * @brief Reads from file descriptor and writes it to a buffer.
 *
 * @param fdRead FD to read from.
 * @param buf Pointer to buffer.
 * @param sz Size of buffer.
 */
void FDBufferWriter(const int fdRead, char* const buf, const size_t sz) {
  ssize_t nBytes = 0;
  uint64_t nTotal = 0;
  while ((nBytes = read(fdRead, buf + nTotal, sz - nTotal)) > 0) {
    nTotal += (uint64_t)nBytes;
  }

  close(fdRead);
}

/**
 * @brief Converts FDBufferWriter to match the FnFDOutput signature.
 *
 * @param buf Pointer to buffer where data is to be written.
 * @param sz Size of the buffer.
 *
 * @return Returns FDBufferWriter matching the FnFDOutput signature.
 */
FnFDOutput GetFDBufferWriter(char* const buf, const size_t sz) {
  return std::bind(FDBufferWriter, std::placeholders::_1, buf, sz);
}

class FDRedirector {
  private:
    // Copy & duplicate of original FD to be redirected
    int fdCopy_ = -1; // Original FD #
    int fdDup_ = -1;  // Duplicated (new FD #) returned from dup()

    // Pipe FDs and aliases to both ends
    int fdPipe_[2] = {-1, -1};
    int& fdRead_ = fdPipe_[0];
    int& fdWrite_ = fdPipe_[1];

    // Boolean flag indicating object's redirection status
    volatile bool redirected_ = false;

    // Stream to print error messages to
    FILE* errStream_ = stderr;

    // Mutex in case of multithreading
    std::mutex mtx_;

    // Inherit std::thread so we can define a destructor.
    // We define it here since it is not intended for use outside the
    // anonymous class.
    std::thread writerThread_;

  public:
    void redirect(const int fd, FnFDOutput outputFunc) {
      std::lock_guard<std::mutex> lock(mtx_);

      if (redirected_ == true) {
        fprintf(errStream_,
                "ERROR: A file descriptor is already redirected\n");
        return;
      }

      // Define stream to print errors to
      errStream_ = fd == STDERR_FILENO ? stdout : stderr;

      // Copy original FD and also duplicate it
      fdCopy_ = fd;
      fdDup_ = dup(fd);
      if (fdCopy_ == -1) {
        fprintf(errStream_, "ERROR: dup() failure; %s\n", strerror(errno));
        return;
      }

      // Create pipe
      if (pipe(fdPipe_) == -1) {
        fprintf(errStream_, "ERROR: pipe() failure; %s\n", strerror(errno));
        return;
      }

      // Replace fd with writer-end of the pipe
      if (dup2(fdWrite_, fd) == -1) {
        fprintf(errStream_, "ERROR: dup2() failure; %s\n", strerror(errno));
        return;
      }

      // Spin up thread to read from reader-end of pipe & write into buffer
      writerThread_ = std::thread(outputFunc, fdRead_);

      redirected_ = true;
    }

    void restore() {
      std::lock_guard<std::mutex> lock(mtx_);

      if (redirected_ == false) {
        fprintf(errStream_, "ERROR: Nothing to restore, "
                "no file descriptor has been redirected\n");
        return;
      }

      if (writerThread_.joinable()) {
        fsync(fdWrite_);
        dup2(fdDup_, fdCopy_);

        close(fdWrite_);
        writerThread_.join();

        fdWrite_ = -1;
        fdRead_ = -1;
      }

      redirected_ = false;
    }

    FDRedirector() {}

    ~FDRedirector() {
      if (redirected_ == true) {
        fprintf(errStream_, "WARNING: Redirector going out-of-scope without"
                "explicit call to restore()\n");
      }

      // Attempt to stop writer thread if it's still open
      if (writerThread_.joinable()) {
        fsync(fdWrite_);
        dup2(fdDup_, fdCopy_);

        close(fdWrite_);
        writerThread_.join();
      }
    }
};

//gStdoutRedir, gStderrRedir;


int main() {
  char buffer[BUFSIZ] = {0};
  FDRedirector redir;

  /* STDERR TEST */
  // Redirect to local buffer
  //StderrToBuf(buffer, BUFSIZ);
  redir.redirect(STDERR_FILENO, GetFDBufferWriter(buffer, BUFSIZ));

  cerr << "ERROR: test string here" << " and here" << endl;

  // Restore stderr
  //RestoreStderr();
  redir.restore();

  /* STDOUT TEST */
  // Redirect to local buffer
  //StdoutToBuf(buffer, BUFSIZ);
  //redir.redirect(STDOUT_FILENO, GetFDBufferWriter(buffer, BUFSIZ));

  //cout << "ERROR: test string here"; cout.flush(); cout << endl;
  //printf("ERROR: test string here"); fflush(stdout); printf("\n");

  //// Restore stdout
  //RestoreStdout();
  //redir.restore();

  printf("buffer contains: %s\n", buffer);


  return 0;
}
