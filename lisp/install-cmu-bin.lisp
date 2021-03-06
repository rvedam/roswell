(in-package :ros.install)
(ros:quicklisp :environment nil)

(defun cmu-bin-get-version ()
  (let ((file (merge-pathnames "tmp/cmu-bin.html" (homedir))))
    (format *error-output* "Checking version to install...~%")
    (unless (and (probe-file file)
                 (< (get-universal-time) (+ (* 60 60) (file-write-date file))))
      (download (format nil "~Arelease/" (cmu-bin-uri))  file))
    (loop for link in (plump:get-elements-by-tag-name (plump:parse file) "a")
       for href = (plump:get-attribute link "href")
       for len = (length href)
       when (and (digit-char-p (aref href 0))
                 (char= (aref href (1- len)) #\/))
       collect (string-right-trim "/" href))))

(defvar *cmu-uname-m-alist*
  '(("x86-64" . "x86")))

(defun cmu-uname-m ()
  (or (cdr (assoc (uname-m) *cmu-uname-m-alist* :test 'equal))
      (uname-m)))

(defun cmu-bin-archive-uri (version extra)
  (let ((uname (uname))
        (cmu-uname-m (cmu-uname-m)))
    (format nil "~@{~A~}" (cmu-bin-uri) (if (find #\- version) "snapshots/" "release/")
            (substitute #\/ #\- version) "/cmucl-" version "-" cmu-uname-m "-" uname (if extra ".extra" "") ".tar.bz2")))

(defun cmu-bin-argv-parse (argv)
  (format *error-output* "~&Installing cmu-bin/~A...~%" (getf argv :version))
  (set-opt "as" (getf argv :version))
  (when (position "--without-install" (getf argv :argv) :test 'equal)
    (set-opt "without-install" t))
  (cons t argv))

(defun cmu-bin-download (argv)
  (set-opt "download.uri" (cmu-bin-archive-uri (getf argv :version) nil))
  (set-opt "download.archive" (let ((pos (position #\/ (get-opt "download.uri") :from-end t)))
                                (when pos 
                                  (merge-pathnames (format nil "archives/~A" (subseq (get-opt "download.uri") (1+ pos))) (homedir)))))
  (set-opt "download.extra.uri" (cmu-bin-archive-uri (getf argv :version) t))
  (set-opt "download.extra.archive" (let ((pos (position #\/ (get-opt "download.extra.uri") :from-end t)))
                                      (when pos 
                                        (merge-pathnames (format nil "archives/~A" (subseq (get-opt "download.extra.uri") (1+ pos))) (homedir)))))
  `((,(get-opt "download.archive") ,(get-opt "download.uri"))
    (,(get-opt "download.extra.archive") ,(get-opt "download.extra.uri"))))

(defun cmu-bin-expand (argv)
  (loop for archive in (list (get-opt "download.archive") (get-opt "download.extra.archive"))
        do (format t "~%Extracting archive:~A~%" (get-opt "download.archive"))
           (let* ((impls (merge-pathnames (format nil "impls/~A/~A/cmu-bin/~A/" (uname-m) (uname) (get-opt "as")) (homedir)))
                  (path (merge-pathnames (format nil "~A/" (get-opt "as")) impls)))
             (expand archive (ensure-directories-exist impls))
             (and (probe-file path)
                  (uiop/filesystem:delete-directory-tree 
                   path :validate t))))
  (cons t argv))

(defun cmu-bin-help (argv)
  (format t "cmu-bin install options~%")
  (flet ((fmt (param default more)
           (format t "--~A~A ~A~%~5T~A~%"
                   (cond ((eql default t) "without-")
                         ((null default) "with-")
                         (t ""))
                   param
                   (or (and (not (null default))
                            (not (eql default t))
                            default)
                       "")
                   more)))
    (fmt "install" t "Download archive"))
  (cons t argv))

(push `("cmu-bin" . (,(decide-version 'cmu-bin-get-version)
                      cmu-bin-argv-parse
                      ,(decide-download 'cmu-bin-download)
                      cmu-bin-expand
                      setup))
      *install-cmds*)

(push `("cmu-bin" . ,(list 'cmu-bin-help)) *help-cmds*)
(push `("cmu-bin" . cmu-bin-get-version) *list-cmd*)
